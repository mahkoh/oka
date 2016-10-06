#pragma once

#include <stdbool.h>

#include "utils/utils.h"
#include "utils/audio.h"
#include "utils/metadata.h"

struct decoder_stream {
    struct audio_format fmt;

    void (*close)(struct decoder_stream *);
    int (*seek)(struct decoder_stream *, i64 diff, u64 *pos, bool *eof);
    int (*seek_abs)(struct decoder_stream *, u64 pos, u64 *opos);
    int (*read)(struct decoder_stream *, u8 *buf, size_t *len, u64 *pos);
};

struct decoder {
    const char *name;

    int (*free)(struct decoder *);
    struct decoder_stream *(*open)(struct decoder *, const char *path,
            const struct audio_format_range *);
    int (*metadata)(struct decoder *d, const char *path,
            char *metadata[static METADATA_NUM_TAGS]);
};

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
