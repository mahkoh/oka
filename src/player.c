#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>

#include "utils/utils.h"
#include "utils/list.h"
#include "utils/delegate.h"
#include "utils/loop.h"
#include "utils/xmalloc.h"
#include "utils/signals.h"
#include "utils/thread.h"
#include "utils/diag.h"

#include "player.h"
#include "globals.h"
#include "sink.h"
#include "decoder.h"
#include "main.h"

struct player_input {
    struct list node;
    struct decoder_stream *stream;
    bool eof;
    i64 remaining_ms;
    struct main_track_cookie *cookie;
    u64 pos_samples;
};

static pthread_t player_thread;
static struct sink *player_sink;
static struct list player_inputs;
static struct loop *player_loop;
static struct loop_defer *player_provide_input_defer;
static bool player_paused;
static bool player_mute;

static struct loop_timer *player_pos_timer;
static u64 player_pos_update_time;
static u32 player_pos_msec;
static u32 player_pos_sec = (u32)-1;

static struct loop_timer *player_track_change_timer;
static u64 player_track_change_update_time;

static struct player_input *player_node_to_input(struct list *node)
{
    return container_of(node, struct player_input, node);
}

static struct player_input *player_first_input(void)
{
    if (list_empty(&player_inputs))
        return NULL;
    return player_node_to_input(player_inputs.next);
}

static struct player_input *player_last_input(void)
{
    if (list_empty(&player_inputs))
        return NULL;
    return player_node_to_input(player_inputs.prev);
}

static void player_timing_update(bool seeked)
{
    auto input = player_first_input();

    if (!input) {
        player_pos_msec = 0;
        player_pos_sec = 0;
    }

    if (player_paused || !input || !player_sink) {
        loop_timer_disable(player_pos_timer);
        main_position_changed(player_pos_sec);
        return;
    }

    player_pos_update_time = utils_get_mono_time_ms();
    player_pos_msec = (u32)(1000 * input->pos_samples / input->stream->fmt.sample_rate);
    u32 latency;

    if (input->eof) {
        auto diff = (u32)(player_pos_update_time - player_track_change_update_time);
        latency = (u32)input->remaining_ms - diff;
        if (diff > input->remaining_ms)
            latency = 0;
    } else
        latency = player_sink->latency(player_sink);

    if (player_pos_msec > latency)
        player_pos_msec -= latency;
    else
        player_pos_msec = 0;
    auto new_sec = player_pos_msec / 1000;
    if (new_sec != player_pos_sec && (new_sec > player_pos_sec || seeked)) {
        player_pos_sec = new_sec;
        main_position_changed(new_sec);
    }

    auto rem_msec = 1000 - (player_pos_msec % 1000);
    if (rem_msec == 1000)
        rem_msec = 0;
    auto rem_sec = player_pos_sec - new_sec + (rem_msec == 0 ? 1 : 0);

    struct itimerspec timer = {
        .it_interval = {
            .tv_sec = 1,
            .tv_nsec = 0,
        },
        .it_value = {
            .tv_sec = rem_sec,
            .tv_nsec = 1000 * 1000 * rem_msec,
        },
    };

    loop_timer_set(player_pos_timer, &timer, false);
}

static void player_pos_tick(struct loop_timer *t, void *opaque)
{
    (void)t;
    (void)opaque;

    if (player_paused)
        return;

    auto old = player_pos_update_time;
    player_pos_update_time = utils_get_mono_time_ms();
    player_pos_msec += player_pos_update_time - old;
    auto new_sec = player_pos_msec / 1000;
    if (new_sec != player_pos_sec) {
        player_pos_sec = new_sec;
        main_position_changed(new_sec);
    }
}

static void player_input_free(struct player_input *input)
{
    input->stream->close(input->stream);
    list_remove(&input->node);
    free(input);
}

static void player_track_change(struct player_input *input)
{
    main_track_changed(input->cookie);
    player_input_free(input);
    player_timing_update(true);
}

static void player_continue_track_change_timer(void)
{
    if (player_paused)
        return;

    struct player_input *first;

    while (1) {
        first = player_first_input();
        if (!first || !first->eof)
            return;
        if (first->remaining_ms <= 0)
            player_track_change(first);
        else
            break;
    }

    long rem_sec = first->remaining_ms / 1000;
    long rem_msec = first->remaining_ms % 1000;
    struct itimerspec timer = {
        .it_interval = { 0 },
        .it_value = {
            .tv_sec = rem_sec,
            .tv_nsec = 1000 * 1000 * rem_msec,
        },
    };
    loop_timer_set(player_track_change_timer, &timer, false);
}

static void player_track_change_tick(struct loop_timer *t, void *opaque)
{
    (void)t;
    (void)opaque;

    auto old = player_track_change_update_time;
    player_track_change_update_time = utils_get_mono_time_ms();
    auto diff = player_track_change_update_time - old;

    struct player_input *input;
    list_for_each_entry(input, &player_inputs, node)
        input->remaining_ms -= diff;

    struct list *node, *n;
    list_for_each_safe(node, n, &player_inputs) {
        input = player_node_to_input(node);
        if (input->remaining_ms > 0 || !input->eof)
            break;
        player_track_change(input);
    }

    player_continue_track_change_timer();
}

static void player_pause_track_change_timer(void)
{
    loop_timer_disable(player_track_change_timer);
    player_track_change_tick(player_track_change_timer, NULL);
}

static void player_start_track_change_timer(void)
{
    player_track_change_update_time = utils_get_mono_time_ms();
    player_continue_track_change_timer();
}

static void player_sink_stop(void)
{
    loop_defer_set(player_provide_input_defer, false);
    player_sink->stop(player_sink);
}

static void player_sink_disable(void)
{
    player_sink->disable(player_sink);
}

static void player_sink_load(struct sink *sink)
{
    if (player_sink) {
        player_sink_stop();
        player_sink_disable();
    }
    player_sink = sink;
    if (player_sink) {
        player_sink->enable(player_sink);
        auto last = player_last_input();
        if (last)
            player_sink->start(player_sink, &last->stream->fmt);
    }
}

static void player_input_load(struct decoder_stream *s, struct main_track_cookie *c,
        bool flush)
{
    bool was_playing = !list_empty(&player_inputs);
    bool is_playing = s != NULL;

    if (flush) {
        struct list *node, *n;
        list_for_each_safe(node, n, &player_inputs) {
            auto input = player_node_to_input(node);
            player_input_free(input);
        }
    }

    if (list_empty(&player_inputs))
        main_track_changed(c);

    auto last = xnew0(struct player_input);
    last->stream = s;
    last->cookie = c;
    list_append(&player_inputs, &last->node);

    if (player_sink) {
        if (was_playing && flush)
            player_sink->flush(player_sink, &s->fmt);
        else if (is_playing)
            player_sink->start(player_sink, &s->fmt);
        else if (was_playing)
            player_sink_stop();
    }

    player_timing_update(true);
}

static void player_goto_next_(bool flush)
{
    struct decoder_stream *next;
    struct main_track_cookie *cookie;
    main_get_next_track_SYNC(&next, &cookie);
    player_input_load(next, cookie, flush);
}

static void player_input_eof(void)
{
    auto last = player_last_input();
    last->remaining_ms = player_sink->latency(player_sink);
    last->eof = true;
    player_start_track_change_timer();
    player_timing_update(false);

    player_goto_next_(false);
}

static void player_provide_input(struct loop_defer *d, void *opaque)
{
    (void)d;
    (void)opaque;

    u8 *buf;
    size_t len;
    BUG_ON(player_sink->provide_buf(player_sink, &buf, &len));

    if (len == 0) {
        BUG_ON(player_sink->commit_buf(player_sink, buf, len));
        loop_defer_set(player_provide_input_defer, false);
        return;
    }

    auto last = player_last_input();

    BUG_ON(last->stream->read(last->stream, buf, &len, &last->pos_samples));
    BUG_ON(player_sink->commit_buf(player_sink, buf, len));

    if (len > 0)
        player_timing_update(false);
    else
        player_input_eof();

    loop_force_iteration(player_loop);
}

static void *player_run(void *opaque)
{
    (void)opaque;

    loop_run(player_loop);

    player_sink_load(NULL);
    player_input_load(NULL, NULL, false);

    return NULL;
}

void player_init(void)
{
    player_loop = loop_new();
    player_provide_input_defer = loop_defer_new(player_loop, player_provide_input, NULL);
    loop_defer_set(player_provide_input_defer, false);
    player_pos_timer = loop_timer_new(player_loop, player_pos_tick, CLOCK_REALTIME, NULL);
    player_track_change_timer = loop_timer_new(player_loop, player_track_change_tick,
            CLOCK_REALTIME, NULL);
    list_init(&player_inputs);
    auto_restore sigs = signals_block_all();
    thread_create(&player_thread, NULL, player_run, NULL);
}

static void player_delegate(struct delegate *d)
{
    loop_delegate(player_loop, d);
}

static void player_exit_delegate(struct delegate *d)
{
    (void)d;
    loop_stop(player_loop, 0);
}

void player_exit(void)
{
    static struct delegate d = { player_exit_delegate };
    player_delegate(&d);
    thread_join(player_thread, NULL);
    loop_free(player_loop);
}

static void player_toggle_pause_delegate(struct delegate *d)
{
    (void)d;
    if (player_sink)
        player_sink->pause(player_sink, !player_paused);
}

void player_toggle_pause(void)
{
    static struct delegate d = { player_toggle_pause_delegate };
    player_delegate(&d);
}

static void player_toggle_mute_delegate(struct delegate *d)
{
    (void)d;
    if (player_sink)
        player_sink->mute(player_sink, !player_mute);
}

void player_toggle_mute(void)
{
    static struct delegate d = { player_toggle_mute_delegate };
    player_delegate(&d);
}

struct player_seek {
    struct delegate d;
    i64 diff;
};

static void player_seek_delegate(struct delegate *d)
{
    auto_free auto seek = container_of(d, struct player_seek, d);

    auto first = player_first_input();
    if (!first)
        return;

    u32 latency = 0;
    if (player_sink) {
        latency = player_sink->latency(player_sink);
        player_sink->flush(player_sink, &first->stream->fmt);
    }

    list_remove(&first->node);
    struct list *node, *n;
    list_for_each_safe(node, n, &player_inputs) {
        auto input = player_node_to_input(node);
        player_input_free(input);
    }
    list_append(&player_inputs, &first->node);

    first->eof = false;
    player_pause_track_change_timer();

    first->stream->seek(first->stream, seek->diff - (i64)latency, &first->pos_samples);
    player_timing_update(true);
}

void player_seek(i64 diff)
{
    auto seek = xnew(struct player_seek);
    seek->d.run = player_seek_delegate;
    seek->diff = diff;
    player_delegate(&seek->d);
}

struct player_set_sink {
    struct delegate d;
    struct sink *sink;
};

static void player_set_sink_delegate(struct delegate *d)
{
    auto_free auto op = container_of(d, struct player_set_sink, d);
    player_sink_load(op->sink);
}

void player_set_sink(struct sink *sink)
{
    auto d = xnew(struct player_set_sink);
    d->d.run = player_set_sink_delegate;
    d->sink = sink;
    player_delegate(&d->d);
}

struct player_set_input {
    struct delegate d;
    struct decoder_stream *s;
    struct main_track_cookie *c;
};

static void player_set_input_delegate(struct delegate *d)
{
    auto_free auto input = container_of(d, struct player_set_input, d);
    player_input_load(input->s, input->c, true);
}

void player_set_input(struct decoder_stream *s, struct main_track_cookie *c)
{
    auto d = xnew(struct player_set_input);
    d->d.run = player_set_input_delegate;
    d->s = s;
    d->c = c;
    player_delegate(&d->d);
}

static void player_goto_next_delegate(struct delegate *d)
{
    (void)d;
    player_goto_next_(true);
}

void player_goto_next(void)
{
    static struct delegate d = { player_goto_next_delegate };
    player_delegate(&d);
}

static void player_stop_delegate(struct delegate *d)
{
    (void)d;
    player_sink_stop();
}

void player_stop(void)
{
    static struct delegate d = { player_stop_delegate };
    player_delegate(&d);
}

static int player_sink_request_input(struct sink *sink, bool request)
{
    BUG_ON(player_sink != sink);

    loop_defer_set(player_provide_input_defer, request);
    if (request) {
        loop_force_iteration(player_loop);
    }

    return 0;
}

static int player_sink_info_changed(struct sink *sink, struct sink_info *i)
{
    BUG_ON(player_sink != sink);

    player_paused = i->paused;
    player_mute = i->mute;

    if (player_paused)
        player_pause_track_change_timer();
    else
        player_start_track_change_timer();

    player_timing_update(false);

    main_sink_info_changed(i);
    return 0;
}

static struct sink_ops player_sink_ops = {
    .request_input = player_sink_request_input,
    .info_changed = player_sink_info_changed,
};

void player_get_sink_ops(const struct sink_ops **ops, struct loop **loop)
{
    *ops = &player_sink_ops;
    *loop = player_loop;
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
