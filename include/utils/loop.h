#ifndef OKA_UTILS_LOOP_H
#define OKA_UTILS_LOOP_H

#include <time.h>
#include <stdbool.h>

#include "utils/utils.h"
#include "utils/delegate.h"

struct loop;
struct loop_watch;
struct loop_defer;
struct loop_timer;

typedef void (*loop_watch_cb)(struct loop_watch *, void *, int, u32);
typedef void (*loop_defer_cb)(struct loop_defer *, void *);
typedef void (*loop_timer_cb)(struct loop_timer *, void *);

struct loop *loop_new(void);
int loop_run(struct loop *loop);
void loop_free(struct loop *loop);

void loop_delegate(struct loop *loop, struct delegate *d);
void loop_delegate_sync(struct loop *loop, struct delegate *d);

// NOTE: None of the following functions are thread-safe.

void loop_stop(struct loop *loop, int ret);
void loop_force_iteration(struct loop *loop);

struct loop_watch *loop_watch_new(struct loop *loop, loop_watch_cb cb, void *opaque);
void loop_watch_set(struct loop_watch *w, int fd, u32 events);
void loop_watch_disable(struct loop_watch *w);
void loop_watch_free(struct loop_watch *w);

struct loop_defer *loop_defer_new(struct loop *loop, loop_defer_cb cb, void *opaque);
void loop_defer_set(struct loop_defer *defer, bool enabled);
void loop_defer_free(struct loop_defer *defer);

struct loop_timer *loop_timer_new(struct loop *loop, loop_timer_cb cb, int clock,
        void *opaque);
void loop_timer_set(struct loop_timer *timer, const struct itimerspec *ts, bool abs);
void loop_timer_disable(struct loop_timer *timer);
void loop_timer_free(struct loop_timer *timer);

#endif

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
