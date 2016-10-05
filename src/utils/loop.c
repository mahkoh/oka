#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>

#include "utils/utils.h"
#include "utils/loop.h"
#include "utils/delegate.h"
#include "utils/channel.h"
#include "utils/xmalloc.h"
#include "utils/debug.h"
#include "utils/vec.h"

UTILS_VECTOR(loop_defer, struct loop_defer *)
UTILS_VECTOR(loop_watch, struct loop_watch *)
UTILS_VECTOR(loop_timer, struct loop_timer *)

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
    struct loop_watch watch;
    loop_timer_cb cb;
};

struct loop {
    int epfd;

    struct delegator *delegator;
    struct loop_defer_vector deferred;

    struct loop_watch_vector freed_watches;
    struct loop_timer_vector freed_timers;

    bool force_iteration;
    bool running;
    int ret;

    struct loop_watch *delegator_watch;
};

void loop_free(struct loop *loop)
{
    loop_watch_free(loop->delegator_watch);

    loop_collect_garbage(loop);

    free(loop->freed_watches.ptr);
    free(loop->freed_timers.ptr);
    free(loop->deferred.ptr);

    delegator_free(loop->delegator);
    close(loop->epfd);

    free(loop);
}

static void loop_handle_delegate(struct loop_watch *w, void *opaque, int fd, u32 event)
{
    (void)w;
    (void)fd;
    (void)event;

    struct loop *loop = opaque;
    delegator_run(loop->delegator);
}

struct loop *loop_new(void)
{
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    BUG_ON(epfd == -1);

    auto loop = xnew0(struct loop);
    loop->epfd = epfd;
    loop->delegator = delegator_new();
    loop->running = true;
    loop->delegator_watch = loop_watch_new(loop, loop_handle_delegate, loop);

    loop_watch_set(loop->delegator_watch, delegator_fd(loop->delegator), EPOLLIN);

    return loop;
}

static void loop_handle_fd(struct epoll_event *event)
{
    struct loop_watch *w = event->data.ptr;
    if (w->fd != -1) {
        w->cb(w, w->opaque, w->fd, event->events);
    }
}

static void loop_run_deferred(struct loop *loop)
{
    for (size_t i = 0; i < loop->deferred.len; i++) {
        auto d = loop->deferred.ptr[i];
        if (d->enabled)
            d->cb(d, d->opaque);
    }
}

static void loop_collect_garbage(struct loop *loop)
{
    for (size_t i = 0; i < loop->freed_watches.len; i++) {
        free(loop->freed_watches.ptr[i]);
    }
    loop->freed_watches.len = 0;

    for (size_t i = 0; i < loop->freed_timers.len; i++) {
        free(loop->freed_timers.ptr[i]);
    }
    loop->freed_timers.len = 0;

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
        loop_collect_garbage(loop);

        int timeout = loop->force_iteration ? 0 : -1;
        loop->force_iteration = false;

        struct epoll_event events[10];
        ssize_t num = epoll_wait(loop->epfd, events, N_ELEMENTS(events), timeout);
        if (num < 0) {
            BUG_ON(num != -EINTR);
            continue;
        }
        for (size_t i = 0; i < (size_t)num; i++) {
            loop_handle_fd(&events[i]);
        }
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

static void loop_watch_init(struct loop_watch *w, struct loop *loop, loop_watch_cb cb,
        void *opaque)
{
    w->loop = loop;
    w->fd = -1;
    w->cb = cb;
    w->opaque = opaque;
}

struct loop_watch *loop_watch_new(struct loop *loop, loop_watch_cb cb, void *opaque)
{
    auto watch = xnew0(struct loop_watch);
    loop_watch_init(watch, loop, cb, opaque);
    return watch;
}

void loop_watch_set(struct loop_watch *w, int fd, u32 events)
{
    struct epoll_event e = { .data.ptr = w, .events = events };

    if (w->fd != fd) {
        loop_watch_disable(w);
        if (fd != -1) {
            BUG_ON(epoll_ctl(w->loop->epfd, EPOLL_CTL_ADD, fd, &e));
        }
    } else if (fd != -1) {
        BUG_ON(epoll_ctl(w->loop->epfd, EPOLL_CTL_MOD, fd, &e));
    }

    w->fd = fd;
}

void loop_watch_disable(struct loop_watch *w)
{
    if (w->fd != -1) {
        BUG_ON(epoll_ctl(w->loop->epfd, EPOLL_CTL_DEL, w->fd, NULL));
        w->fd = -1;
    }
}

void loop_watch_free(struct loop_watch *w)
{
    loop_watch_disable(w);
    loop_watch_vector_push(&w->loop->freed_watches, w);
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

static void loop_timer_handle(struct loop_watch *w, void *opaque, int fd, u32 event)
{
    (void)event;

    auto timer = container_of(w, struct loop_timer, watch);

    u64 exp;
    if (read(fd, &exp, sizeof(exp)) == -1) {
        BUG_ON(errno != EAGAIN);
    }

    for (size_t i = 0; i < exp; i++) {
        timer->cb(timer, opaque);
    }
}

struct loop_timer *loop_timer_new(struct loop *loop, loop_timer_cb cb, int clock,
        void *opaque)
{
    int fd = timerfd_create(clock, TFD_NONBLOCK | TFD_CLOEXEC);
    BUG_ON(fd == -1);

    auto t = xnew0(struct loop_timer);

    loop_watch_init(&t->watch, loop, loop_timer_handle, opaque);
    t->cb = cb;

    loop_watch_set(&t->watch, fd, EPOLLIN);

    return t;
}

void loop_timer_set(struct loop_timer *timer, const struct itimerspec *ts, bool abs)
{
    BUG_ON(timerfd_settime(timer->watch.fd, abs ? TFD_TIMER_ABSTIME : 0, ts, NULL));
}

void loop_timer_disable(struct loop_timer *timer)
{
    struct itimerspec ts = { { 0 } };
    loop_timer_set(timer, &ts, false);
}

void loop_timer_free(struct loop_timer *timer)
{
    close(timer->watch.fd);
    timer->watch.fd = -1;
    loop_timer_vector_push(&timer->watch.loop->freed_timers, timer);
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
