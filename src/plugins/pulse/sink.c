#include "utils/audio.h"

#include "plugin.h"

#include "pulse.h"

static int pulse_sink_enable(struct sink *sink)
{
    return pulse_ctx_enable(pulse_ctx_from_sink(sink));
}

static int pulse_sink_start(struct sink *sink, const struct audio_format *f)
{
    pulse_ctx_set_input_format(pulse_ctx_from_sink(sink), f);
    return 0;
}

static int pulse_sink_stop(struct sink *sink)
{
    pulse_ctx_set_input_format(pulse_ctx_from_sink(sink), NULL);
    return 0;
}

static int pulse_sink_disable(struct sink *sink)
{
    pulse_ctx_disable(pulse_ctx_from_sink(sink));
    return 0;
}

static int pulse_sink_provide_buf(struct sink *sink, u8 **buf, size_t *len)
{
    return pulse_ctx_provide_buf(pulse_ctx_from_sink(sink), buf, len);
}

static int pulse_sink_commit_buf(struct sink *sink, u8 *buf, size_t len)
{
    return pulse_ctx_commit_buf(pulse_ctx_from_sink(sink), buf, len);
}

static int pulse_sink_pause(struct sink *sink, bool pause)
{
    pulse_ctx_set_pause(pulse_ctx_from_sink(sink), pause);
    return 0;
}

static int pulse_sink_mute(struct sink *sink, bool mute)
{
    pulse_ctx_set_mute(pulse_ctx_from_sink(sink), mute);
    return 0;
}

static int pulse_sink_flush(struct sink *sink, const struct audio_format *f)
{
    pulse_ctx_flush(pulse_ctx_from_sink(sink), f);
    return 0;
}

static u32 pulse_sink_latency(struct sink *sink)
{
    return pulse_ctx_latency(pulse_ctx_from_sink(sink));
}

static int pulse_sink_free(struct sink *sink)
{
    return pulse_ctx_free(pulse_ctx_from_sink(sink));
}

struct sink pulse_sink_template = {
    .name = "pulse",

    .range = (struct audio_format_range) {
        .sample_fmts = AUDIO_FORMAT_ALAW | AUDIO_FORMAT_ULAW |
            AUDIO_FORMAT_S16_LE | AUDIO_FORMAT_S16_BE | AUDIO_FORMAT_S24_LE |
            AUDIO_FORMAT_S24_BE | AUDIO_FORMAT_S24_IN_32_LE |
            AUDIO_FORMAT_S24_IN_32_BE | AUDIO_FORMAT_S32_LE |
            AUDIO_FORMAT_S32_BE | AUDIO_FORMAT_U8 | AUDIO_FORMAT_FLOAT32_LE |
            AUDIO_FORMAT_FLOAT32_BE,
        .min_sample_rate = PULSE_MIN_SAMPLE_RATE,
        .max_sample_rate = PULSE_MAX_SAMPLE_RATE,
        .min_channels = PULSE_MIN_CHANNELS,
        .max_channels = PULSE_MAX_CHANNELS,
    },

    .enable = pulse_sink_enable,
    .disable = pulse_sink_disable,
    .free = pulse_sink_free,

    .start = pulse_sink_start,
    .stop = pulse_sink_stop,
    .pause = pulse_sink_pause,
    .mute = pulse_sink_mute,

    .provide_buf = pulse_sink_provide_buf,
    .commit_buf = pulse_sink_commit_buf,
    .flush = pulse_sink_flush,
    .latency = pulse_sink_latency,
};

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
