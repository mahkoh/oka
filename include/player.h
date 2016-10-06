#pragma once

#include <time.h>
#include <stdbool.h>

#include "utils/loop.h"

#include "sink.h"
#include "decoder.h"
#include "main.h"

void player_init(void);
void player_exit(void);
void player_set_sink(struct sink *sink);
void player_set_input(struct decoder_stream *, struct main_track_cookie *);
void player_get_sink_ops(const struct sink_ops **ops, struct loop **loop);
void player_toggle_pause(void);
void player_toggle_mute(void);
void player_seek(i64 diff);
void player_goto_next(void);
void player_stop(void);

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
