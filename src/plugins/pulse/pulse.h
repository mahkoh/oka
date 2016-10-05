#ifndef OKA_PLUGINS_PULSE_PULSE_H
#define OKA_PLUGINS_PULSE_PULSE_H

#include <pulse/pulseaudio.h>

#include "plugin.h"

#define auto_op_unref \
    __attribute__((cleanup(pulse_pa_operation_unrefp), unused)) pa_operation *

struct pulse_ctx {
    pa_context *ctx;
    struct diag *diag;
    pa_mainloop_api mainloop_api;
    struct loop *loop;
};

enum pulse_stream_state {
    PULSE_STREAM_CONNECTING,
    PULSE_STREAM_READY,
    PULSE_STREAM_DEAD,
};

struct pulse_stream {
    u32 index;
    enum pulse_stream_state state;
    struct sink_info info;
    struct audio_format fmt;
    size_t requested_bytes;
};

#pragma GCC visibility push(hidden)

#define PULSE_MIN_SAMPLE_RATE 1
#define PULSE_MAX_SAMPLE_RATE 192000
#define PULSE_MIN_CHANNELS 1
#define PULSE_MAX_CHANNELS 32

extern pa_mainloop_api pulse_mainloop_template;
extern struct sink pulse_sink_template;

struct pulse_ctx *pulse_ctx_from_sink(struct sink *);
int pulse_ctx_enable(struct pulse_ctx *c);
int pulse_ctx_disable(struct pulse_ctx *cc);
int pulse_ctx_set_input_format(struct pulse_ctx *cc, const struct audio_format *f);
int pulse_ctx_flush(struct pulse_ctx *cc, const struct audio_format *f);
int pulse_ctx_set_pause(struct pulse_ctx *c, bool pause);
int pulse_ctx_set_mute(struct pulse_ctx *c, bool mute);
int pulse_ctx_provide_buf(struct pulse_ctx *c, u8 **buf, size_t *len);
int pulse_ctx_commit_buf(struct pulse_ctx *c, u8 *buf, size_t len);
u32 pulse_ctx_latency(struct pulse_ctx *c);
int pulse_ctx_free(struct pulse_ctx *c);
const char *pulse_ctx_last_err(struct pulse_ctx *c);
void pulse_ctx_get_sink_input_info(struct pulse_ctx *c, struct pulse_stream *s);
const char *pulse_ctx_latest_error(struct pulse_ctx *cc);
void pulse_ctx_stream_state_changed(struct pulse_ctx *c, struct pulse_stream *s);
void pulse_ctx_stream_drained(struct pulse_ctx *cc, struct pulse_stream *s);
void pulse_ctx_stream_request_changed(struct pulse_ctx *cc, struct pulse_stream *s);
void pulse_ctx_stream_info_changed(struct pulse_ctx *c, struct pulse_stream *s);
int pulse_ctx_add_sink(const struct plugin_ops *ops, struct diag *diag);

struct pulse_stream *pulse_stream_new(struct pulse_ctx *c, const struct audio_format *f,
        bool corked);
void pulse_stream_free(struct pulse_stream *ss);
void pulse_stream_flush(struct pulse_stream *ss);
int pulse_stream_commit_buf(struct pulse_stream *ss, u8 *buf, size_t len);
int pulse_stream_provide_buf(struct pulse_stream *ss, u8 **buf, size_t *len);
pa_usec_t pulse_stream_latency(struct pulse_stream *ss);
void pulse_stream_set_drain(struct pulse_stream *ss, bool drain);
void pulse_stream_query_info(struct pulse_stream *ss);
void pulse_stream_set_mute(struct pulse_stream *ss, bool mute);
void pulse_stream_set_pause(struct pulse_stream *ss, bool pause);

void pulse_utils_audio_fmt_to_sample_spec(pa_sample_spec *spec,
        const struct audio_format *fmt);

#pragma GCC visibility pop

#endif

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
