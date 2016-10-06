#pragma once

#include <stdbool.h>

struct channel;

typedef bool (*channel_remove_cb)(void *, void *opaque);

struct channel *channel_new(bool need_pipe);
void channel_free(struct channel *c);
void channel_push(struct channel *c, void *data);
void *channel_pop__(struct channel *c);
void *channel_pop_wait__(struct channel *c);
void channel_remove(struct channel *c, channel_remove_cb cb, void *opaque);
int channel_fd(struct channel *c);
void channel_clear_fd(struct channel *c);

#define channel_pop(c, type) (type *)channel_pop__(c)
#define channel_pop_wait(c, type) (type *)channel_pop_wait__(c)

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
