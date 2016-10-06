#include <mpg123.h>
#include <stdio.h>
#include <pthread.h>

#include "utils/utils.h"
#include "utils/xmalloc.h"
#include "utils/thread.h"
#include "utils/id3.h"
#include "utils/diag.h"

#include "plugin.h"

#define auto_delete __attribute__((cleanup(ip_mpg123_deletep))) mpg123_handle *

static pthread_once_t ip_init_once = PTHREAD_ONCE_INIT;
static bool ip_inited;
static struct diag *ip_diag;

struct ip_stream {
    struct decoder_stream d;
    mpg123_handle *h;
};

static void ip_mpg123_deletep(mpg123_handle **p)
{
    if (*p)
        mpg123_delete(*p);
}

static struct ip_stream *ip_to_stream(struct decoder_stream *d)
{
    return container_of(d, struct ip_stream, d);
}

static void ip_close(struct decoder_stream *d)
{
    auto_free auto s = ip_to_stream(d);
    mpg123_close(s->h);
    mpg123_delete(s->h);
}

static int ip_read(struct decoder_stream *d, u8 *buf, size_t *len, u64 *pos)
{
    auto s = ip_to_stream(d);
    size_t tlen = 0;

    *pos = (u64)mpg123_tell(s->h);

    do {
        auto rc = mpg123_read(s->h, buf, *len, &tlen);
        if (rc == MPG123_OK || rc == MPG123_NEW_FORMAT)
            ;
        else if (rc == MPG123_DONE)
            break;
        else
            return -1;
    } while (tlen == 0);

    *len = tlen;
    return 0;
}

static int ip_seek(struct decoder_stream *d, i64 diff, u64 *pos, bool *eof)
{
    auto s = ip_to_stream(d);

    *pos = (u64)mpg123_seek(s->h, (diff * 44100) / 1000, SEEK_CUR);
    *eof = mpg123_read(s->h, NULL, 0, NULL) == MPG123_DONE;
    return 0;
}

static int ip_seek_abs(struct decoder_stream *d, u64 pos, u64 *opos)
{
    auto s = ip_to_stream(d);

    *opos = (u64)mpg123_seek(s->h, (pos * 44100) / 1000, SEEK_SET);
    return 0;
}

static int ip_free(struct decoder *d)
{
    free(d);
    if (ip_inited)
        mpg123_exit();
    return 0;
}

static void ip_init(void)
{
    mpg123_init();
}

static const struct {
    audio_sample_fmt_type native;
    int mpg123;
} ip_format_map[] = {
    { AUDIO_FORMAT_S16,     MPG123_ENC_SIGNED_16   },
    { AUDIO_FORMAT_U16,     MPG123_ENC_UNSIGNED_16 },
    { AUDIO_FORMAT_U8,      MPG123_ENC_UNSIGNED_8  },
    { AUDIO_FORMAT_S8,      MPG123_ENC_SIGNED_8    },
    { AUDIO_FORMAT_ULAW,    MPG123_ENC_ULAW_8      },
    { AUDIO_FORMAT_ALAW,    MPG123_ENC_ALAW_8      },
    { AUDIO_FORMAT_S32,     MPG123_ENC_SIGNED_32   },
    { AUDIO_FORMAT_U32,     MPG123_ENC_UNSIGNED_32 },
    { AUDIO_FORMAT_S24,     MPG123_ENC_SIGNED_24   },
    { AUDIO_FORMAT_U24,     MPG123_ENC_UNSIGNED_24 },
    { AUDIO_FORMAT_FLOAT32, MPG123_ENC_FLOAT_32    },
    { AUDIO_FORMAT_FLOAT64, MPG123_ENC_FLOAT_64    },
};

static int ip_set_range(mpg123_handle *h, const struct audio_format_range *range)
{
    if (!range)
        return 0;

    auto fmts = range->sample_fmts;
    int fmt = 0;

    for (size_t i = 0; i < N_ELEMENTS(ip_format_map); i++) {
        if (fmts & ip_format_map[i].native)
            fmt |= ip_format_map[i].mpg123;
    }

    int channels = 0;
    if (range->min_channels <= 1 && 1 <= range->max_channels)
        channels |= MPG123_MONO;
    if (range->min_channels <= 2 && 2 <= range->max_channels)
        channels |= MPG123_STEREO;

    const long *rates;
    size_t num_rates;
    mpg123_rates(&rates, &num_rates);

    if (mpg123_format_none(h) != MPG123_OK)
        return -1;

    for (size_t i = 0; i < num_rates; i++) {
        if (range->min_sample_rate <= rates[i] && rates[i] <= range->max_sample_rate) {
            if (mpg123_format(h, rates[i], channels, fmt) != MPG123_OK)
                return -1;
        }
    }

    return 0;
}

static int ip_fix_format(mpg123_handle *h, struct audio_format *format)
{
    long rate;
    int channels, encoding;

    if (mpg123_getformat(h, &rate, &channels, &encoding) != MPG123_OK)
        return -1;
    if (mpg123_format_none(h) != MPG123_OK)
        return -1;
    if (mpg123_format(h, rate, channels, encoding) != MPG123_OK)
        return -1;

    if (channels == MPG123_MONO)
        format->channels = 1;
    else if (channels == MPG123_STEREO)
        format->channels = 2;
    else
        return -1;

    format->sample_rate = (u32)rate;
    format->sample_fmt = 0;

    for (size_t i = 0; i < N_ELEMENTS(ip_format_map); i++) {
        if (encoding == ip_format_map[i].mpg123) {
            format->sample_fmt = ip_format_map[i].native;
            break;
        }
    }

    if (format->sample_fmt == 0)
        return -1;

    return 0;
}

static struct decoder_stream *ip_open(struct decoder *d, const char *path,
        const struct audio_format_range *range)
{
    (void)d;

    thread_once(&ip_init_once, ip_init);

    auto_delete h = mpg123_new(NULL, NULL);
    if (!h)
        return NULL;
    if (mpg123_open(h, path) != MPG123_OK)
        return NULL;
    if (ip_set_range(h, range))
        return NULL;

    struct audio_format format;

    if (ip_fix_format(h, &format))
        return NULL;

    auto s = xnew_uninit(struct ip_stream);
    s->h = move(h);
    s->d.close = ip_close;
    s->d.seek = ip_seek;
    s->d.seek_abs = ip_seek_abs;
    s->d.read = ip_read;
    s->d.fmt = format;

    return &s->d;
}

static int ip_metadata(struct decoder *d, const char *path,
        char *metadata[static METADATA_NUM_TAGS])
{
    (void)d;

    thread_once(&ip_init_once, ip_init);

    auto_delete h = mpg123_new(NULL, NULL);
    if (!h)
        return -1;
    if (mpg123_open(h, path) != MPG123_OK)
        return -1;
    mpg123_scan(h);
    mpg123_id3v2 *v2;
    mpg123_id3(h, NULL, &v2);
    for (size_t i = 0; i < v2->texts; i++) {
        auto text = &v2->text[i];
        if (text->text.size == 0)
            continue;
        id3_metadata(text->id, text->text.p, metadata);
    }
    return 0;
}

static int plugin_init(const struct plugin_ops *ops, struct diag *diag)
{
    ip_diag = diag;

    auto decoder = xnew_uninit(struct decoder);
    decoder->name = "mpg123";
    decoder->free = ip_free;
    decoder->open = ip_open;
    decoder->metadata = ip_metadata;

    BUG_ON(ops->add_decoder(decoder));

    return 0;
}

static void plugin_exit(void)
{
}

const struct plugin_api plugin_api = {
    .version = PLUGIN_API_VERSION,

    .init = plugin_init,
    .exit = plugin_exit,
};

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
