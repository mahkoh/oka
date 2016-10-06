#include <sys/epoll.h>
#include <pulse/pulseaudio.h>
#include <stdio.h>

#include "utils/utils.h"
#include "utils/xmalloc.h"
#include "utils/loop.h"

#include "pulse.h"

struct pa_io_event {
    struct pulse_ctx *ctx;
    pa_io_event_cb_t cb;
    pa_io_event_destroy_cb_t destroy_cb;
    void *userdata;
    int fd;
    struct loop_watch *watch;
};

struct pa_defer_event {
    struct pulse_ctx *ctx;
    pa_defer_event_cb_t cb;
    pa_defer_event_destroy_cb_t destroy_cb;
    void *userdata;
    struct loop_defer *defer;
};

struct pa_time_event {
    struct pulse_ctx *ctx;
    pa_time_event_cb_t cb;
    pa_time_event_destroy_cb_t destroy_cb;
    void *userdata;
    struct loop_timer *timer;
    struct timeval tv;
};

static u32 pulse_mainloop_io_event_flags_to_poll_flags(pa_io_event_flags_t pa_flags)
{
    u32 flags = 0;
    if (pa_flags & PA_IO_EVENT_INPUT)  { flags |= EPOLLIN;  }
    if (pa_flags & PA_IO_EVENT_OUTPUT) { flags |= EPOLLOUT; }
    return flags;
}

static pa_io_event_flags_t pulse_poll_flags_to_pa_io_event_flags(u32 flags)
{
    auto events = (pa_io_event_flags_t)PA_IO_EVENT_NULL;
    if (flags & EPOLLIN)  { events |= PA_IO_EVENT_INPUT;  }
    if (flags & EPOLLOUT) { events |= PA_IO_EVENT_OUTPUT; }
    if (flags & EPOLLHUP) { events |= PA_IO_EVENT_HANGUP; }
    if (flags & EPOLLERR) { events |= PA_IO_EVENT_ERROR;  }
    return events;
}

static void pulse_mainloop_watch_cb(struct loop_watch *w, void *opaque, int fd, u32 flags)
{
    (void)w;

    struct pa_io_event *p = opaque;
    auto pa_flags = pulse_poll_flags_to_pa_io_event_flags(flags);
    p->cb(&p->ctx->mainloop_api, p, fd, pa_flags, p->userdata);
}

static void pulse_mainloop_io_enable(pa_io_event *e, pa_io_event_flags_t events)
{
    if (events == PA_IO_EVENT_NULL) {
        loop_watch_disable(e->watch);
    } else {
        auto flags = pulse_mainloop_io_event_flags_to_poll_flags(events);
        loop_watch_set(e->watch, e->fd, flags);
    }
}

static pa_io_event *pulse_mainloop_io_new(pa_mainloop_api *a, int fd,
        pa_io_event_flags_t events, pa_io_event_cb_t cb, void *userdata)
{
    struct pulse_ctx *ctx = a->userdata;

    auto event = xnew0(struct pa_io_event);
    event->ctx = ctx;
    event->cb = cb;
    event->userdata = userdata;
    event->fd = fd;
    event->watch = loop_watch_new(ctx->loop, pulse_mainloop_watch_cb, event);

    pulse_mainloop_io_enable(event, events);

    return event;
}

static void pulse_mainloop_io_set_destroy(pa_io_event *e, pa_io_event_destroy_cb_t cb)
{
    e->destroy_cb = cb;
}

static void pulse_mainloop_io_free(pa_io_event *e)
{
    if (e->destroy_cb) {
        e->destroy_cb(&e->ctx->mainloop_api, e, e->userdata);
    }
    loop_watch_free(e->watch);
    free(e);
}

static void pulse_mainloop_timer_cb(struct loop_timer *t, void *opaque)
{
    (void)t;

    struct pa_time_event *time = opaque;
    time->cb(&time->ctx->mainloop_api, time, &time->tv, time->userdata);
}

static void pulse_mainloop_time_restart(pa_time_event *e, const struct timeval *tv)
{
    struct itimerspec ts = { { 0 }, { 0 } };

    if (tv) {
        ts.it_value.tv_sec = tv->tv_sec,
        ts.it_value.tv_nsec = tv->tv_usec * 1000,
        e->tv = *tv;
    }

    loop_timer_set(e->timer, &ts, true);
}

static pa_time_event *pulse_mainloop_time_new(pa_mainloop_api *a,
        const struct timeval *tv, pa_time_event_cb_t cb, void *userdata)
{
    struct pulse_ctx *ctx = a->userdata;

    auto timer = xnew0(struct pa_time_event);
    timer->ctx = ctx;
    timer->cb = cb;
    timer->userdata = userdata;
    timer->timer = loop_timer_new(ctx->loop, pulse_mainloop_timer_cb, CLOCK_REALTIME,
            timer);

    pulse_mainloop_time_restart(timer, tv);

    return timer;
}

static void pulse_mainloop_time_set_destroy(pa_time_event *e,
        pa_time_event_destroy_cb_t cb)
{
    e->destroy_cb = cb;
}

static void pulse_mainloop_time_free(pa_time_event *e)
{
    if (e->destroy_cb) {
        e->destroy_cb(&e->ctx->mainloop_api, e, e->userdata);
    }
    loop_timer_free(e->timer);
    free(e);
}

static void pulse_mainloop_defer_cb(struct loop_defer *d, void *opaque)
{
    (void)d;

    struct pa_defer_event *defer = opaque;
    defer->cb(&defer->ctx->mainloop_api, defer, defer->userdata);
}

static pa_defer_event *pulse_mainloop_defer_new(pa_mainloop_api *a,
        pa_defer_event_cb_t cb, void *userdata)
{
    struct pulse_ctx *ctx = a->userdata;

    auto defer = xnew0(struct pa_defer_event);
    defer->ctx = ctx;
    defer->cb = cb;
    defer->userdata = userdata;
    defer->defer = loop_defer_new(ctx->loop, pulse_mainloop_defer_cb, defer);

    return defer;
}

static void pulse_mainloop_defer_enable(pa_defer_event *e, int b)
{
    loop_defer_set(e->defer, b);
}

static void pulse_mainloop_defer_set_destroy(pa_defer_event *e,
        pa_defer_event_destroy_cb_t cb)
{
    e->destroy_cb = cb;
}

static void pulse_mainloop_defer_free(pa_defer_event *e)
{
    if (e->destroy_cb) {
        e->destroy_cb(&e->ctx->mainloop_api, e, e->userdata);
    }
    loop_defer_free(e->defer);
    free(e);
}

static void pulse_mainloop_quit(pa_mainloop_api *a, int code)
{
    (void)a;
    (void)code;
}

pa_mainloop_api pulse_mainloop_template = {
    .io_new            = pulse_mainloop_io_new,
    .io_enable         = pulse_mainloop_io_enable,
    .io_free           = pulse_mainloop_io_free,
    .io_set_destroy    = pulse_mainloop_io_set_destroy,

    .time_new          = pulse_mainloop_time_new,
    .time_restart      = pulse_mainloop_time_restart,
    .time_free         = pulse_mainloop_time_free,
    .time_set_destroy  = pulse_mainloop_time_set_destroy,

    .defer_new         = pulse_mainloop_defer_new,
    .defer_enable      = pulse_mainloop_defer_enable,
    .defer_free        = pulse_mainloop_defer_free,
    .defer_set_destroy = pulse_mainloop_defer_set_destroy,

    .quit              = pulse_mainloop_quit,
};

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
