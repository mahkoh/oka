#include <stdbool.h>

#include "utils/utils.h"
#include "utils/audio.h"

bool audio_format_included(const struct audio_format_range *range,
        const struct audio_format *fmt)
{
    return (range->sample_fmts & fmt->sample_fmt) &&
        (range->min_sample_rate <= fmt->sample_rate) &&
        (range->max_sample_rate >= fmt->sample_rate) &&
        (range->min_channels <= fmt->channels) &&
        (range->max_channels >= fmt->channels);
}

size_t audio_bytes_per_sample(audio_sample_fmt_type type)
{
    switch (type) {
    case AUDIO_FORMAT_ALAW: return 1;
    case AUDIO_FORMAT_ULAW: return 1;
    case AUDIO_FORMAT_S8: return 1;
    case AUDIO_FORMAT_S16_LE: return 2;
    case AUDIO_FORMAT_S16_BE: return 2;
    case AUDIO_FORMAT_S24_LE: return 3;
    case AUDIO_FORMAT_S24_BE: return 3;
    case AUDIO_FORMAT_S24_IN_32_LE: return 4;
    case AUDIO_FORMAT_S24_IN_32_BE: return 4;
    case AUDIO_FORMAT_S32_LE: return 4;
    case AUDIO_FORMAT_S32_BE: return 4;
    case AUDIO_FORMAT_U8: return 1;
    case AUDIO_FORMAT_U16_LE: return 2;
    case AUDIO_FORMAT_U16_BE: return 2;
    case AUDIO_FORMAT_U24_LE: return 3;
    case AUDIO_FORMAT_U24_BE: return 3;
    case AUDIO_FORMAT_U24_IN_32_LE: return 4;
    case AUDIO_FORMAT_U24_IN_32_BE: return 4;
    case AUDIO_FORMAT_U32_LE: return 4;
    case AUDIO_FORMAT_U32_BE: return 4;
    case AUDIO_FORMAT_FLOAT32_LE: return 4;
    case AUDIO_FORMAT_FLOAT32_BE: return 4;
    case AUDIO_FORMAT_FLOAT64_LE: return 8;
    case AUDIO_FORMAT_FLOAT64_BE: return 8;
    }
    BUG("unexpected audio format");
}

bool audio_formats_eq(const struct audio_format *l, const struct audio_format *r)
{
    return l->sample_fmt == r->sample_fmt &&
        l->sample_rate == r->sample_rate &&
        l->channels == r->channels;
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
