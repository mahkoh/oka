#include <pulse/pulseaudio.h>

#include "utils/utils.h"
#include "utils/list.h"
#include "utils/vec.h"

#include "plugin.h"
#include "pulse.h"

UTILS_VECTOR(pulse_stream_ptr, struct pulse_stream *)

enum pulse_ctx_state {
    PULSE_CTX_DISABLED,
    PULSE_CTX_ENABLED,
    PULSE_CTX_READY,
};

struct pulse_ctx_priv {
    struct pulse_ctx pub;
    const struct sink_ops *ops;
    struct pulse_stream_ptr_vector streams;
    struct pulse_stream *input;
    enum pulse_ctx_state state;
    bool mute;
    bool paused;
    struct audio_format fmt;
    bool have_fmt;
    struct sink sink;
};

static struct pulse_ctx_priv *pulse_ctx_to_priv(struct pulse_ctx *cc)
{
    return container_of(cc, struct pulse_ctx_priv, pub);
}

static void pulse_ctx_set_input(struct pulse_ctx_priv *c, struct pulse_stream *s)
{
    c->input = s;
    c->fmt = s->fmt;
    c->have_fmt = true;
    c->ops->request_input(&c->sink, s->requested_bytes != 0);
}

static void pulse_ctx_create_input_stream(struct pulse_ctx_priv *c,
        const struct audio_format *f)
{
    auto start_corked = c->streams.len > 0 || c->paused;
    auto s = pulse_stream_new(&c->pub, f, start_corked);
    pulse_ctx_set_input(c, s);
    pulse_stream_ptr_vector_push(&c->streams, s);
}

const char *pulse_ctx_latest_error(struct pulse_ctx *cc)
{
    auto c = pulse_ctx_to_priv(cc);

    if (c->state == PULSE_CTX_DISABLED) {
        return "";
    } else {
        return pa_strerror(pa_context_errno(cc->ctx));
    }
}

static void pulse_ctx_on_subscription_event(pa_context *pa_ctx,
        pa_subscription_event_type_t t, uint32_t idx, void *userdata)
{
    (void)pa_ctx;

    auto facility = t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
    auto type = t & PA_SUBSCRIPTION_EVENT_TYPE_MASK;
    if (facility != PA_SUBSCRIPTION_EVENT_SINK_INPUT ||
            type != PA_SUBSCRIPTION_EVENT_CHANGE)
    {
        return;
    }

    struct pulse_ctx_priv *c = userdata;

    for (size_t i = 0; i < c->streams.len; i++) {
        auto s = c->streams.ptr[i];
        if (s->index == idx) {
            pulse_stream_query_info(s);
            break;
        }
    }
}

static void pulse_ctx_on_ready(struct pulse_ctx_priv *c)
{
    BUG_ON(c->state != PULSE_CTX_ENABLED);

    pa_context_set_subscribe_callback(c->pub.ctx, pulse_ctx_on_subscription_event, c);
    auto op = pa_context_subscribe(c->pub.ctx, PA_SUBSCRIPTION_MASK_SINK_INPUT, NULL,
            NULL);
    pa_operation_unref(op);

    c->state = PULSE_CTX_READY;
    if (c->have_fmt) {
        pulse_ctx_create_input_stream(c, &c->fmt);
    }
}

static void pulse_ctx_init(struct pulse_ctx_priv *c, struct diag *diag)
{
    *c = (struct pulse_ctx_priv) {
        .sink = pulse_sink_template,
        .pub.mainloop_api = pulse_mainloop_template,
        .pub.diag = diag,
        .state = PULSE_CTX_DISABLED,
    };
    c->pub.mainloop_api.userdata = &c->pub;
}

static void pulse_ctx_on_failed(struct pulse_ctx_priv *c)
{
    BUG_ON(c->state == PULSE_CTX_DISABLED);

    const char *prefix = c->state == PULSE_CTX_ENABLED ? "cannot connect" :
        "connection failed";
    diag_err(c->pub.diag, "pulse: %s: %s", prefix, pulse_ctx_latest_error(&c->pub));

    for (size_t i = 0; i < c->streams.len; i++) {
        pulse_stream_free(c->streams.ptr[i]);
    }
    pa_context_unref(c->pub.ctx);

    auto retry = c->state == PULSE_CTX_READY;
    auto ops = c->ops;

    pulse_ctx_init(c, c->pub.diag);

    ops->failed(&c->sink, retry);
}

static void pulse_ctx_on_state_change(pa_context *pa_ctx, void *userdata)
{
    struct pulse_ctx_priv *c = userdata;
    auto state = pa_context_get_state(pa_ctx);

    if (state == PA_CONTEXT_FAILED) {
        pulse_ctx_on_failed(c);
    } else if (state == PA_CONTEXT_READY) {
        pulse_ctx_on_ready(c);
    }
}

struct pulse_ctx *pulse_ctx_from_sink(struct sink *sink)
{
    return &container_of(sink, struct pulse_ctx_priv, sink)->pub;
}

int pulse_ctx_provide_buf(struct pulse_ctx *cc, u8 **buf, size_t *len)
{
    auto c = pulse_ctx_to_priv(cc);

    BUG_ON(c->state != PULSE_CTX_READY);
    BUG_ON(!c->input);

    return pulse_stream_provide_buf(c->input, buf, len);
}

int pulse_ctx_commit_buf(struct pulse_ctx *cc, u8 *buf, size_t len)
{
    auto c = pulse_ctx_to_priv(cc);

    BUG_ON(c->state != PULSE_CTX_READY);
    BUG_ON(!c->input);

    return pulse_stream_commit_buf(c->input, buf, len);
}

static struct pulse_stream *pulse_ctx_playing_stream(struct pulse_ctx_priv *c)
{
    return c->streams.len > 0 ? c->streams.ptr[0] : NULL;
}

int pulse_ctx_set_pause(struct pulse_ctx *cc, bool pause)
{
    auto c = pulse_ctx_to_priv(cc);

    c->paused = pause;
    auto s = pulse_ctx_playing_stream(c);
    if (s) {
        pulse_stream_set_pause(s, pause);
    }

    return 0;
}

int pulse_ctx_set_mute(struct pulse_ctx *cc, bool mute)
{
    auto c = pulse_ctx_to_priv(cc);

    c->mute = mute;
    auto s = pulse_ctx_playing_stream(c);
    if (s) {
        pulse_stream_set_mute(s, mute);
    }

    return 0;
}

int pulse_ctx_flush(struct pulse_ctx *cc, const struct audio_format *f)
{
    auto c = pulse_ctx_to_priv(cc);

    if (c->state == PULSE_CTX_READY) {
        for (size_t i = 1; i < c->streams.len; i++) {
            pulse_stream_free(c->streams.ptr[i]);
        }
        c->streams.len = min(c->streams.len, (size_t)1);
        c->input = NULL;
        c->have_fmt = false;

        auto s = pulse_ctx_playing_stream(c);
        if (s) {
            if (f && audio_formats_eq(&s->fmt, f)) {
                pulse_stream_set_drain(s, false);
                if (s->state == PULSE_STREAM_READY) {
                    pulse_stream_flush(s);
                }
                pulse_ctx_set_input(c, s);
            } else {
                pulse_stream_free(s);
                c->streams.len = 0;
            }
        }
    }

    pulse_ctx_set_input_format(cc, f);
    return 0;
}

u32 pulse_ctx_latency(struct pulse_ctx *cc)
{
    auto c = pulse_ctx_to_priv(cc);

    pa_usec_t total = 0;
    if (c->state == PULSE_CTX_READY) {
        for (size_t i = 0; i < c->streams.len; i++) {
            total += pulse_stream_latency(c->streams.ptr[i]);
        }
    }
    return (u32)(total / 1000);
}

void pulse_ctx_stream_state_changed(struct pulse_ctx *c, struct pulse_stream *s)
{
    (void)c;
    if (s->state == PULSE_STREAM_DEAD) {
        BUG("pulse stream died");
    }
}

void pulse_ctx_stream_drained(struct pulse_ctx *cc, struct pulse_stream *s)
{
    auto c = pulse_ctx_to_priv(cc);

    if (s == pulse_ctx_playing_stream(c)) {
        pulse_stream_free(s);
        pulse_stream_ptr_vector_remove(&c->streams, 0);
    }

    s = pulse_ctx_playing_stream(c);
    if (s) {
        pulse_stream_set_mute(s, c->mute);
        pulse_stream_set_pause(s, c->paused);
    }
}

void pulse_ctx_stream_request_changed(struct pulse_ctx *cc, struct pulse_stream *s)
{
    auto c = pulse_ctx_to_priv(cc);

    if (s == c->input) {
        c->ops->request_input(&c->sink, s->requested_bytes != 0);
    } else {
        pulse_stream_set_drain(s, true);
    }
}

void pulse_ctx_stream_info_changed(struct pulse_ctx *cc, struct pulse_stream *s)
{
    auto c = pulse_ctx_to_priv(cc);

    if (s != pulse_ctx_playing_stream(c)) {
        return;
    }

    c->paused = s->info.paused;
    c->mute = s->info.mute;

    c->ops->info_changed(&c->sink, &s->info);
}

static void pulse_ctx_drain(struct pulse_ctx_priv *c)
{
    if (c->streams.len != 0) {
        pulse_stream_set_drain(c->streams.ptr[0], true);
    }
}

int pulse_ctx_disable(struct pulse_ctx *cc)
{
    auto c = pulse_ctx_to_priv(cc);

    if (c->state == PULSE_CTX_DISABLED) {
        return 0;
    }

    pa_context_set_subscribe_callback(cc->ctx, NULL, NULL);
    pa_context_set_state_callback(cc->ctx, NULL, NULL);

    for (size_t i = 0; i < c->streams.len; i++) {
        pulse_stream_free(c->streams.ptr[i]);
    }

    pa_context_disconnect(cc->ctx);
    pa_context_unref(cc->ctx);

    pulse_ctx_init(c, c->pub.diag);
    return 0;
}

int pulse_ctx_set_input_format(struct pulse_ctx *cc, const struct audio_format *f)
{
    auto c = pulse_ctx_to_priv(cc);

    if (c->state == PULSE_CTX_ENABLED) {
        c->have_fmt = f != NULL;
        if (f) {
            c->fmt = *f;
        }
    } else if (c->state == PULSE_CTX_READY) {
        if (c->have_fmt && f && audio_formats_eq(&c->fmt, f)) {
            return 0;
        }

        pulse_ctx_drain(c);
        c->input = NULL;

        c->have_fmt = f != NULL;
        if (f) {
            pulse_ctx_create_input_stream(c, f);
        }
    } else {
        BUG("invalid state");
    }

    return 0;
}

static int pulse_pa_error(int rc)
{
    (void)rc;
    return -1;
}

int pulse_ctx_enable(struct pulse_ctx *cc)
{
    auto c = pulse_ctx_to_priv(cc);

    BUG_ON(c->state != PULSE_CTX_DISABLED);

    cc->ctx = pa_context_new(&cc->mainloop_api, "oka");
    pa_context_set_state_callback(cc->ctx, pulse_ctx_on_state_change, c);
    auto rc = pa_context_connect(cc->ctx, NULL, PA_CONTEXT_NOFLAGS, NULL);
    if (rc) {
        pa_context_unref(cc->ctx);
        cc->ctx = NULL;
        return pulse_pa_error(rc);
    }

    c->state = PULSE_CTX_ENABLED;
    return 0;
}

int pulse_ctx_free(struct pulse_ctx *cc)
{
    auto c = pulse_ctx_to_priv(cc);

    pulse_ctx_disable(cc);
    free(c);

    return 0;
}

int pulse_ctx_add_sink(const struct plugin_ops *ops, struct diag *diag)
{
    auto c = xnew_uninit(struct pulse_ctx_priv);

    pulse_ctx_init(c, diag);

    if (ops->add_sink(&c->sink, &c->ops, &c->pub.loop)) {
        diag_err(diag, "pulse: unable to register sink");
        pulse_ctx_free(&c->pub);
        return -1;
    }

    return 0;
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
