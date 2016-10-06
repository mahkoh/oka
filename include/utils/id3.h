#pragma once

#include "utils/metadata.h"

void id3_metadata(const char tag[static 4], const char *txt,
        char *metadata[static METADATA_NUM_TAGS]);

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
