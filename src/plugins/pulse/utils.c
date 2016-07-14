#include <stddef.h>
#include <pulse/pulseaudio.h>

#include "utils/utils.h"
#include "utils/audio.h"
#include "pulse.h"

void pulse_utils_audio_fmt_to_sample_spec(pa_sample_spec *spec,
        const struct audio_format *fmt)
{
    static const struct {
        audio_sample_fmt_type native;
        pa_sample_format_t pa;
    } map[] = {
        { AUDIO_FORMAT_ALAW,         PA_SAMPLE_ALAW      },
        { AUDIO_FORMAT_ULAW,         PA_SAMPLE_ULAW      },
        { AUDIO_FORMAT_S16_LE,       PA_SAMPLE_S16LE     },
        { AUDIO_FORMAT_S16_BE,       PA_SAMPLE_S16BE     },
        { AUDIO_FORMAT_S24_LE,       PA_SAMPLE_S24LE     },
        { AUDIO_FORMAT_S24_BE,       PA_SAMPLE_S24BE     },
        { AUDIO_FORMAT_S24_IN_32_LE, PA_SAMPLE_S24_32LE  },
        { AUDIO_FORMAT_S24_IN_32_BE, PA_SAMPLE_S24_32BE  },
        { AUDIO_FORMAT_S32_LE,       PA_SAMPLE_S32LE     },
        { AUDIO_FORMAT_S32_BE,       PA_SAMPLE_S32BE     },
        { AUDIO_FORMAT_U8,           PA_SAMPLE_U8        },
        { AUDIO_FORMAT_FLOAT32_LE,   PA_SAMPLE_FLOAT32LE },
        { AUDIO_FORMAT_FLOAT32_BE,   PA_SAMPLE_FLOAT32BE },
    };

    BUG_ON(fmt->sample_rate < PULSE_MIN_SAMPLE_RATE);
    BUG_ON(fmt->sample_rate > PULSE_MAX_SAMPLE_RATE);
    spec->rate = fmt->sample_rate;

    BUG_ON(fmt->channels < PULSE_MIN_CHANNELS);
    BUG_ON(fmt->channels > PULSE_MAX_CHANNELS);
    spec->channels = (u8)fmt->channels;

    for (size_t i = 0; i < N_ELEMENTS(map); i++) {
        if (map[i].native == fmt->sample_fmt) {
            spec->format = map[i].pa;
            return;
        }
    }

    BUG("incompatible audio format");
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
