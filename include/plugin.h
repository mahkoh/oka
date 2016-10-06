#pragma once

#include <stdbool.h>
#include <time.h>

#include "utils/utils.h"
#include "utils/loop.h"
#include "utils/diag.h"

#include "sink.h"
#include "decoder.h"

#define PLUGIN_API_VERSION 1

struct plugin_ops {
    int (*add_sink)(struct sink *, const struct sink_ops **, struct loop **);
    int (*add_decoder)(struct decoder *);
};

struct plugin_api {
    u32 version;

    int (*init)(const struct plugin_ops *, struct diag *);
    void (*exit)(void);
};

extern const struct plugin_api plugin_api;

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
