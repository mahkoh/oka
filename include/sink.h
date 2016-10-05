#ifndef OKA_SINK_H
#define OKA_SINK_H

#include <stdbool.h>

#include "utils/utils.h"
#include "utils/audio.h"

struct sink {
    const char *name;
    struct audio_format_range range;

    int (*enable)(struct sink *);
    int (*disable)(struct sink *);
    int (*free)(struct sink *);

    int (*set_format)(struct sink *, const struct audio_format *);
    int (*pause)(struct sink *, bool);
    int (*mute)(struct sink *, bool);

    int (*provide_buf)(struct sink *, u8 **, size_t *);
    int (*commit_buf)(struct sink *, u8 *, size_t);
    int (*flush)(struct sink *, const struct audio_format *);
    u32 (*latency)(struct sink *);
};

struct sink_info {
    bool stopped;
    bool paused;
    bool mute;
    u8 vol_l;
    u8 vol_r;
};

struct sink_ops {
    int (*request_input)(struct sink *, bool);
    int (*info_changed)(struct sink *, struct sink_info *);
    int (*failed)(struct sink *, bool retry);
};

#endif

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
