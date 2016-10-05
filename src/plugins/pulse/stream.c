#include <pulse/pulseaudio.h>
#include <stdio.h>

#include "utils/utils.h"
#include "utils/xmalloc.h"
#include "plugin.h"

#include "pulse.h"

struct pulse_stream_priv {
    struct pulse_stream pub;
    struct pulse_ctx *ctx;
    pa_stream *pa_stream;
    struct pa_operation *drain_op;
};

static struct pulse_stream_priv *pulse_stream_to_priv(struct pulse_stream *s)
{
    return container_of(s, struct pulse_stream_priv, pub);
}

static void pulse_stream_set_state(struct pulse_stream_priv *s,
        enum pulse_stream_state state)
{
    s->pub.state = state;
    pulse_ctx_stream_state_changed(s->ctx, &s->pub);
}

static void pulse_stream_free_inner(struct pulse_stream_priv *s)
{
    if (s->drain_op) {
        pa_operation_cancel(s->drain_op);
        pa_operation_unref(s->drain_op);
        s->drain_op = NULL;
    }

    if (s->pa_stream) {
        pa_stream_set_write_callback(s->pa_stream, NULL, NULL);
        pa_stream_set_state_callback(s->pa_stream, NULL, NULL);
        pa_stream_disconnect(s->pa_stream);
        pa_stream_unref(s->pa_stream);
        s->pa_stream = NULL;
    }
}

static void pulse_stream_drain_cb(struct pa_stream *ps, int success, void *userdata)
{
    (void)ps;
    struct pulse_stream_priv *s = userdata;

    if (success) {
        pa_operation_unref(s->drain_op);
        s->drain_op = NULL;
        pulse_ctx_stream_drained(s->ctx, &s->pub);
    }
}

void pulse_stream_set_drain(struct pulse_stream *ss, bool drain)
{
    auto s = pulse_stream_to_priv(ss);

    if (ss->state != PULSE_STREAM_READY) {
        if (drain) {
            pulse_ctx_stream_drained(s->ctx, ss);
        }
        return;
    }

    if (drain) {
        if (!s->drain_op) {
            s->drain_op = pa_stream_drain(s->pa_stream, pulse_stream_drain_cb, s);
        }
    } else if (s->drain_op) {
        pa_operation_cancel(s->drain_op);
        pa_operation_unref(s->drain_op);
        s->drain_op = NULL;
    }
}

pa_usec_t pulse_stream_latency(struct pulse_stream *ss)
{
    auto s = pulse_stream_to_priv(ss);

    pa_usec_t latency = 0;
    if (ss->state == PULSE_STREAM_READY) {
        pa_stream_get_latency(s->pa_stream, &latency, NULL);
    }
    return latency;
}

int pulse_stream_provide_buf(struct pulse_stream *ss, u8 **buf, size_t *len)
{
    auto s = pulse_stream_to_priv(ss);

    BUG_ON(ss->state != PULSE_STREAM_READY);

    *len = (size_t)-1;
    if (pa_stream_begin_write(s->pa_stream, (void *)buf, len) < 0) {
        auto err = pulse_ctx_latest_error(s->ctx);
        diag_err(s->ctx->diag, "pulse: unable to get buffer: %s", err);
        return -1;
    }

    return 0;
}

static void pulse_stream_on_ready(struct pulse_stream_priv *s)
{
    s->pub.index = pa_stream_get_index(s->pa_stream);
    pulse_stream_set_state(s, PULSE_STREAM_READY);
    pulse_stream_query_info(&s->pub);
}

static void pulse_stream_on_failed(struct pulse_stream_priv *s)
{
    pulse_stream_free_inner(s);
    pulse_stream_set_state(s, PULSE_STREAM_DEAD);
}

static void pulse_stream_on_state_changed(pa_stream *p, void *userdata)
{
    struct pulse_stream_priv *s = userdata;

    auto state = pa_stream_get_state(p);

    if (s->pub.state == PULSE_STREAM_CONNECTING) {
        if (state == PA_STREAM_UNCONNECTED || state == PA_STREAM_CREATING) {
            // nothing
        } else if (state == PA_STREAM_READY) {
            pulse_stream_on_ready(s);
        } else if (state == PA_STREAM_FAILED) {
            pulse_stream_on_failed(s);
        } else {
            BUG("unexpected state");
        }
    } else if (s->pub.state == PULSE_STREAM_READY) {
        if (state == PA_STREAM_FAILED) {
            pulse_stream_on_failed(s);
        } else {
            BUG("unexpected state");
        }
    } else {
        BUG("unexpected state");
    }
}

static void pulse_stream_on_writeable(pa_stream *p, size_t nbytes, void *userdata)
{
    (void)p;
    struct pulse_stream_priv *s = userdata;

    s->pub.requested_bytes = nbytes;
    pulse_ctx_stream_request_changed(s->ctx, &s->pub);
}

int pulse_stream_commit_buf(struct pulse_stream *ss, u8 *buf, size_t len)
{
    auto s = pulse_stream_to_priv(ss);

    BUG_ON(ss->state != PULSE_STREAM_READY);

    if (len == 0) {
        pa_stream_cancel_write(s->pa_stream);
        return 0;
    }

    ss->requested_bytes -= min(ss->requested_bytes, len);
    if (ss->requested_bytes == 0) {
        pulse_ctx_stream_request_changed(s->ctx, &s->pub);
    }

    if (pa_stream_write(s->pa_stream, buf, len, NULL, 0, PA_SEEK_RELATIVE) < 0) {
        auto err = pulse_ctx_latest_error(s->ctx);
        diag_err(s->ctx->diag, "pulse: unable to commit buffer: %s", err);
        return -1;
    }

    return 0;
}

void pulse_stream_flush(struct pulse_stream *ss)
{
    auto s = pulse_stream_to_priv(ss);

    BUG_ON(ss->state != PULSE_STREAM_READY);

    auto op = pa_stream_flush(s->pa_stream, NULL, NULL);
    if (!op) {
        auto err = pulse_ctx_latest_error(s->ctx);
        diag_err(s->ctx->diag, "pulse: unable to flush stream: %s", err);
        return;
    }
    pa_operation_unref(op);
}

void pulse_stream_free(struct pulse_stream *ss)
{
    auto s = pulse_stream_to_priv(ss);

    pulse_stream_free_inner(s);
    free(s);
}

static void pulse_stream_connect(struct pulse_stream_priv *s, bool corked)
{
    pa_sample_spec spec;
    pulse_utils_audio_fmt_to_sample_spec(&spec, &s->pub.fmt);

    pa_channel_map map;
    pa_channel_map_init_auto(&map, spec.channels, PA_CHANNEL_MAP_DEFAULT);

    s->pa_stream = pa_stream_new(s->ctx->ctx, "oka", &spec, &map);
    BUG_ON(!s->pa_stream);

    pa_stream_set_write_callback(s->pa_stream, pulse_stream_on_writeable, s);
    pa_stream_set_state_callback(s->pa_stream, pulse_stream_on_state_changed, s);

    auto flags = (pa_stream_flags_t)(PA_STREAM_AUTO_TIMING_UPDATE |
            PA_STREAM_INTERPOLATE_TIMING);

    if (corked) {
        flags |= PA_STREAM_START_CORKED;
    }

    struct pa_buffer_attr attr = {
        .maxlength = (u32)-1,
        .tlength = (u32)-1,
        .prebuf = (u32)-1,
        .minreq = (u32)-1,
        .fragsize = (u32)-1,
    };

    auto res = pa_stream_connect_playback(s->pa_stream, NULL, &attr, flags, NULL, NULL);
    BUG_ON(res != 0);
}

struct pulse_stream *pulse_stream_new(struct pulse_ctx *c, const struct audio_format *f,
        bool corked)
{
    auto s = xnew0(struct pulse_stream_priv);
    s->ctx = c;
    s->pub.fmt = *f;
    s->pub.state = PULSE_STREAM_CONNECTING;

    pulse_stream_connect(s, corked);

    return &s->pub;
}

void pulse_stream_set_mute(struct pulse_stream *ss, bool mute)
{
    auto s = pulse_stream_to_priv(ss);

    auto op = pa_context_set_sink_input_mute(s->ctx->ctx, ss->index, mute, NULL, NULL);
    pa_operation_unref(op);
}

void pulse_stream_set_pause(struct pulse_stream *ss, bool pause)
{
    auto s = pulse_stream_to_priv(ss);

    auto op = pa_stream_cork(s->pa_stream, pause, NULL, NULL);
    pa_operation_unref(op);
}

static void pulse_stream_on_info(pa_context *c, const pa_sink_input_info *i, int eol,
        void *userdata)
{
    (void)c;
    (void)eol;

    if (!i) {
        return;
    }

    pa_channel_map map;
    pa_channel_map_init_auto(&map, 2, PA_CHANNEL_MAP_DEFAULT);

    pa_cvolume vol = i->volume;

    u32 l = pa_cvolume_get_position(&vol, &map, PA_CHANNEL_POSITION_FRONT_LEFT);
    u32 r = pa_cvolume_get_position(&vol, &map, PA_CHANNEL_POSITION_FRONT_RIGHT);

    struct pulse_stream_priv *s = userdata;

    s->pub.info.stopped = false,
    s->pub.info.paused = i->corked,
    s->pub.info.mute = i->mute,
    s->pub.info.vol_l = (u8)((100 * l) / PA_VOLUME_NORM),
    s->pub.info.vol_r = (u8)((100 * r) / PA_VOLUME_NORM),

    pulse_ctx_stream_info_changed(s->ctx, &s->pub);
}

void pulse_stream_query_info(struct pulse_stream *ss)
{
    auto s = pulse_stream_to_priv(ss);

    auto op = pa_context_get_sink_input_info(s->ctx->ctx, ss->index, pulse_stream_on_info,
            s);
    pa_operation_unref(op);
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
