#include <stdbool.h>
#include <stddef.h>

#include "utils/utils.h"
#include "utils/defer.h"
#include "utils/xmalloc.h"
#include "utils/vec.h"

UTILS_VECTOR(deferred, struct deferred *)

struct defer {
    struct deferred_vector vec;
    bool rescheduled;
};

struct deferred {
    struct defer *defer;
    deferred_cb cb;
    void *opaque;
    bool enabled;
    bool freed;
    bool rescheduled;
};

struct defer *defer_new(void)
{
    return xnew0(struct defer);
}

void defer_free(struct defer *d)
{
    for (size_t i = 0; i < d->vec.len; i++)
        free(d->vec.ptr[i]);
    free(d->vec.ptr);
    free(d);
}

struct deferred *defer_add(struct defer *defer, deferred_cb cb, void *opaque)
{
    auto d = xnew0(struct deferred);
    d->defer = defer;
    d->cb = cb;
    d->opaque = opaque;
    d->enabled = true;

    deferred_vector_push(&defer->vec, d);

    return d;
}

void deferred_enable(struct deferred *d, bool b)
{
    d->enabled = b;
}

void deferred_reschedule(struct deferred *d)
{
    d->rescheduled = true;
    d->defer->rescheduled = true;
}

void deferred_free(struct deferred *d)
{
    d->enabled = false;
    d->rescheduled = false;
    d->freed = true;
}

void defer_run(struct defer *defer)
{
    defer->rescheduled = false;

    for (size_t i = 0; i < defer->vec.len; i++) {
        auto d = defer->vec.ptr[i];
        d->rescheduled = false;
        if (d->enabled)
            d->cb(d->opaque);
    }

    while (defer->rescheduled) {
        defer->rescheduled = false;
        for (size_t i = 0; i < defer->vec.len; i++) {
            auto d = defer->vec.ptr[i];
            if (d->enabled && d->rescheduled) {
                d->rescheduled = false;
                d->cb(d->opaque);
            }
        }
    }

    for (size_t i = 0; i < defer->vec.len; i++) {
        auto d = defer->vec.ptr[i];
        if (d->freed) {
            free(d);
            deferred_vector_swap_remove(&defer->vec, i);
            i--;
        }
    }
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
