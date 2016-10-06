#pragma once

#include "utils/loop.h"
#include "sink.h"

void pii_request_input(struct pii_sink *sink);

struct pii_source;

struct pii_decoder_api {
    struct pii_source *(*open)(const char *path, void *);
    void (*close)(struct pii_source *);
    int (*read)(struct pii_source *, u8 *buf, size_t *len);
};

struct pii {
    int (*add_sink)(const char *name, struct sink *sink, const struct sink_api *api,
            const struct sink_formats *formats, struct loop **loop);
    int (*add_decoder)(const char *name, const struct pii_decoder_api *api, void *opaque,
            struct pii_decoder **decoder, struct loop **loop);
};

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
