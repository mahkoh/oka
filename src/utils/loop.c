#include <stdbool.h>
#include <poll.h>
#include <errno.h>

#include "utils/utils.h"
#include "utils/loop.h"
#include "utils/delegate.h"
#include "utils/channel.h"
#include "utils/xmalloc.h"
#include "utils/debug.h"
#include "utils/fdev.h"
#include "utils/timer.h"
#include "utils/vec.h"

UTILS_VECTOR(loop_defer, struct loop_defer *)

struct loop_watch {
    struct loop *loop;
    int fd;
    loop_watch_cb cb;
    void *opaque;
};

struct loop_defer {
    struct loop *loop;
    loop_defer_cb cb;
    void *opaque;
    bool enabled;
    bool freed;
};

struct loop_timer {
    struct loop *loop;
    loop_timer_cb cb;
    void *opaque;
    struct timer *timer;
};

struct loop {
    struct fdev *fds;
    struct channel *timer;
    struct delegator *delegator;
    struct loop_defer_vector deferred;
    const struct timer_api *timer_api;

    bool force_iteration;
    bool running;
    int ret;

    struct loop_watch *timer_watch;
    struct loop_watch *delegator_watch;
};

void loop_free(struct loop *loop)
{
    loop_watch_free(loop->timer_watch);
    loop_watch_free(loop->delegator_watch);

    for (size_t i = 0; i < loop->deferred.len; i++)
        free(loop->deferred.ptr[i]);
    free(loop->deferred.ptr);

    delegator_free(loop->delegator);
    channel_free(loop->timer);
    fdev_free(loop->fds);

    free(loop);
}

static void loop_handle_delegate(struct loop_watch *w, void *opaque, int fd, short event)
{
    (void)w;
    (void)fd;
    (void)event;

    struct loop *loop = opaque;
    delegator_run(loop->delegator);
}

static void loop_handle_timer(struct loop_watch *w, void *opaque, int fd, short event)
{
    (void)w;
    (void)fd;
    (void)event;

    struct loop *loop = opaque;
    channel_clear_fd(loop->timer);

    struct loop_timer *t;
    while ((t = channel_pop(loop->timer, struct loop_timer)))
        t->cb(t, t->opaque);
}

struct loop *loop_new(const struct timer_api *api)
{
    auto loop = xnew0(struct loop);
    loop->fds = fdev_new();
    loop->timer = channel_new(true);
    loop->delegator = delegator_new();
    loop->timer_api = api;

    loop->running = true;

    loop->timer_watch = loop_watch_new(loop, loop_handle_timer, loop);
    loop->delegator_watch = loop_watch_new(loop, loop_handle_delegate, loop);

    loop_watch_set(loop->timer_watch, channel_fd(loop->timer), POLLIN);
    loop_watch_set(loop->delegator_watch, delegator_fd(loop->delegator), POLLIN);

    return loop;
}

static void loop_handle_fd(struct fdev_event *event)
{
    struct loop_watch *w = event->data;
    w->cb(w, w->opaque, w->fd, event->events);
}

static void loop_run_deferred(struct loop *loop)
{
    for (size_t i = 0; i < loop->deferred.len; i++) {
        auto d = loop->deferred.ptr[i];
        if (d->enabled)
            d->cb(d, d->opaque);
    }

    for (size_t i = 0; i < loop->deferred.len; i++) {
        auto d = loop->deferred.ptr[i];
        if (d->freed) {
            free(d);
            loop_defer_vector_swap_remove(&loop->deferred, i);
            i--;
        }
    }
}

int loop_run(struct loop *loop)
{
    while (loop->running) {
        loop_run_deferred(loop);

        int timeout = loop->force_iteration ? 0 : -1;
        loop->force_iteration = false;

        struct fdev_event events[10];
        ssize_t num = fdev_wait(loop->fds, events, 10, timeout);
        if (num < 0) {
            BUG_ON(num != -EINTR);
            continue;
        }
        for (size_t i = 0; i < (size_t)num; i++)
            loop_handle_fd(&events[i]);
    }

    return loop->ret;
}

void loop_stop(struct loop *loop, int ret)
{
    loop->ret = ret;
    loop->running = false;
}

void loop_force_iteration(struct loop *loop)
{
    loop->force_iteration = true;
}

void loop_delegate(struct loop *loop, struct delegate *d)
{
    delegator_delegate(loop->delegator, d);
}

void loop_delegate_sync(struct loop *loop, struct delegate *d)
{
    delegator_delegate_sync(loop->delegator, d);
}

struct loop_watch *loop_watch_new(struct loop *loop, loop_watch_cb cb, void *opaque)
{
    auto watch = xnew0(struct loop_watch);
    watch->loop = loop;
    watch->fd = -1;
    watch->cb = cb;
    watch->opaque = opaque;
    return watch;
}

void loop_watch_set(struct loop_watch *w, int fd, short events)
{
    if (w->fd != fd && w->fd != -1)
        fdev_remove(w->loop->fds, w->fd);
    w->fd = fd;
    if (fd != -1)
        fdev_set(w->loop->fds, fd, w, events);
}

void loop_watch_disable(struct loop_watch *w)
{
    if (w->fd != -1)
        fdev_disable(w->loop->fds, w->fd);
}

void loop_watch_free(struct loop_watch *w)
{
    if (w->fd != -1)
        fdev_remove(w->loop->fds, w->fd);
    free(w);
}

struct loop_defer *loop_defer_new(struct loop *loop, loop_defer_cb cb, void *opaque)
{
    auto d = xnew0(struct loop_defer);
    d->loop = loop;
    d->cb = cb;
    d->opaque = opaque;
    d->enabled = true;
    loop_defer_vector_push(&loop->deferred, d);
    return d;
}

void loop_defer_set(struct loop_defer *defer, bool enabled)
{
    defer->enabled = enabled;
}

void loop_defer_free(struct loop_defer *defer)
{
    defer->enabled = false;
    defer->freed = true;
}

struct loop_timer *loop_timer_new(struct loop *loop, loop_timer_cb cb, void *opaque)
{
    auto t = xnew0(struct loop_timer);
    t->loop = loop;
    t->cb = cb;
    t->opaque = opaque;
    t->timer = loop->timer_api->new(loop->timer, t);
    return t;
}

void loop_timer_set(struct loop_timer *timer, const struct itimerspec *ts, bool abs)
{
    timer->loop->timer_api->set(timer->timer, abs ? TIMER_ABSTIME : 0, ts);
}

void loop_timer_disable(struct loop_timer *timer)
{
    struct itimerspec ts = { { 0 } };
    loop_timer_set(timer, &ts, false);
}

void loop_timer_free(struct loop_timer *timer)
{
    timer->loop->timer_api->free(timer->timer);
    free(timer);
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
