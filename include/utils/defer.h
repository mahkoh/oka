#ifndef OKA_UTILS_DEFER_H
#define OKA_UTILS_DEFER_H

#include <stdbool.h>

typedef void (*deferred_cb)(void *);
struct defer;
struct deferred;

struct defer *defer_new(void);
struct deferred *defer_add(struct defer *, deferred_cb, void *);
void defer_run(struct defer *defer);
void defer_free(struct defer *);

void deferred_enable(struct deferred *, bool);
void deferred_reschedule(struct deferred *);
void deferred_free(struct deferred *);

#endif

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
