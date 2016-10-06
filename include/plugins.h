#pragma once

#include "plugin.h"
#include "decoder.h"

struct plugin_ip_opened;

void plugins_init(void);
void plugins_exit(void);

struct decoder_stream *plugins_open(const char *path);

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
