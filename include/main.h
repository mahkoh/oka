#ifndef OKA_MAIN_H
#define OKA_MAIN_H

#include "utils/utils.h"

#include "sink.h"
#include "decoder.h"

struct main_track_cookie;

void main_sink_info_changed(struct sink_info *i);
void main_position_changed(u32 pos);
void main_track_changed(struct main_track_cookie *);
void main_get_next_track_SYNC(struct decoder_stream **, struct main_track_cookie **);

#endif

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
