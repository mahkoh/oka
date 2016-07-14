#ifndef OKA_UTILS_AUDIO_H
#define OKA_UTILS_AUDIO_H

#include <stdbool.h>

#include "utils/utils.h"

typedef u32 audio_sample_fmt_type;
#define AUDIO_FORMAT(n) ((audio_sample_fmt_type)1 << n)

#define AUDIO_FORMAT_ALAW         AUDIO_FORMAT(0)
#define AUDIO_FORMAT_ULAW         AUDIO_FORMAT(1)
#define AUDIO_FORMAT_S8           AUDIO_FORMAT(2)
#define AUDIO_FORMAT_S16_LE       AUDIO_FORMAT(3)
#define AUDIO_FORMAT_S16_BE       AUDIO_FORMAT(4)
#define AUDIO_FORMAT_S24_LE       AUDIO_FORMAT(5)
#define AUDIO_FORMAT_S24_BE       AUDIO_FORMAT(6)
#define AUDIO_FORMAT_S24_IN_32_LE AUDIO_FORMAT(7)
#define AUDIO_FORMAT_S24_IN_32_BE AUDIO_FORMAT(8)
#define AUDIO_FORMAT_S32_LE       AUDIO_FORMAT(9)
#define AUDIO_FORMAT_S32_BE       AUDIO_FORMAT(10)
#define AUDIO_FORMAT_U8           AUDIO_FORMAT(11)
#define AUDIO_FORMAT_U16_LE       AUDIO_FORMAT(12)
#define AUDIO_FORMAT_U16_BE       AUDIO_FORMAT(13)
#define AUDIO_FORMAT_U24_LE       AUDIO_FORMAT(14)
#define AUDIO_FORMAT_U24_BE       AUDIO_FORMAT(15)
#define AUDIO_FORMAT_U24_IN_32_LE AUDIO_FORMAT(16)
#define AUDIO_FORMAT_U24_IN_32_BE AUDIO_FORMAT(17)
#define AUDIO_FORMAT_U32_LE       AUDIO_FORMAT(18)
#define AUDIO_FORMAT_U32_BE       AUDIO_FORMAT(19)
#define AUDIO_FORMAT_FLOAT32_LE   AUDIO_FORMAT(20)
#define AUDIO_FORMAT_FLOAT32_BE   AUDIO_FORMAT(21)
#define AUDIO_FORMAT_FLOAT64_LE   AUDIO_FORMAT(22)
#define AUDIO_FORMAT_FLOAT64_BE   AUDIO_FORMAT(23)

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
# define AUDIO_FORMAT_U16       AUDIO_FORMAT_U16_BE
# define AUDIO_FORMAT_U24       AUDIO_FORMAT_U24_BE
# define AUDIO_FORMAT_U24_IN_32 AUDIO_FORMAT_U24_IN_32_BE
# define AUDIO_FORMAT_U32       AUDIO_FORMAT_U32_BE
# define AUDIO_FORMAT_S16       AUDIO_FORMAT_S16_BE
# define AUDIO_FORMAT_S24       AUDIO_FORMAT_S24_BE
# define AUDIO_FORMAT_S24_IN_32 AUDIO_FORMAT_S24_IN_32_BE
# define AUDIO_FORMAT_S32       AUDIO_FORMAT_S32_BE
# define AUDIO_FORMAT_FLOAT32   AUDIO_FORMAT_FLOAT32_BE
# define AUDIO_FORMAT_FLOAT64   AUDIO_FORMAT_FLOAT64_BE
#else
# define AUDIO_FORMAT_U16       AUDIO_FORMAT_U16_LE
# define AUDIO_FORMAT_U24       AUDIO_FORMAT_U24_LE
# define AUDIO_FORMAT_U24_IN_32 AUDIO_FORMAT_U24_IN_32_LE
# define AUDIO_FORMAT_U32       AUDIO_FORMAT_U32_LE
# define AUDIO_FORMAT_S16       AUDIO_FORMAT_S16_LE
# define AUDIO_FORMAT_S24       AUDIO_FORMAT_S24_LE
# define AUDIO_FORMAT_S24_IN_32 AUDIO_FORMAT_S24_IN_32_LE
# define AUDIO_FORMAT_S32       AUDIO_FORMAT_S32_LE
# define AUDIO_FORMAT_FLOAT32   AUDIO_FORMAT_FLOAT32_LE
# define AUDIO_FORMAT_FLOAT64   AUDIO_FORMAT_FLOAT64_LE
#endif

struct audio_format {
    audio_sample_fmt_type sample_fmt;
    u32 sample_rate;
    u32 channels;
};

struct audio_format_range {
    audio_sample_fmt_type sample_fmts;
    u32 min_sample_rate;
    u32 max_sample_rate;
    u32 min_channels;
    u32 max_channels;
};

bool audio_format_included(const struct audio_format_range *range,
        const struct audio_format *fmt);

size_t audio_bytes_per_sample(audio_sample_fmt_type);
bool audio_formats_eq(const struct audio_format *l, const struct audio_format *r);

#endif

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
