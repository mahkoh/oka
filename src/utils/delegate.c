#include <stdbool.h>

#include "utils/utils.h"
#include "utils/xmalloc.h"
#include "utils/channel.h"
#include "utils/delegate.h"
#include "utils/thread.h"

struct delegator {
    struct channel *c;
};

struct delegate_sync {
    struct delegate d;
    struct delegate *real;
    struct thread_sync sync;
};

struct delegator *delegator_new(void)
{
    auto d = xnew(struct delegator);
    d->c = channel_new(true);
    return d;
}

int delegator_fd(struct delegator *d)
{
    return channel_fd(d->c);
}

void delegator_run(struct delegator *d)
{
    utils_clear_pipe(channel_fd(d->c));
    struct delegate *dd;
    while ((dd = channel_pop(d->c, struct delegate)))
        dd->run(dd);
}

void delegator_free(struct delegator *d)
{
    channel_free(d->c);
    free(d);
}

void delegator_delegate(struct delegator *d, struct delegate *dd)
{
    channel_push(d->c, dd);
}

static void delegator_delegate_sync_delegate(struct delegate *d)
{
    auto sync = container_of(d, struct delegate_sync, d);
    sync->real->run(sync->real);
    thread_sync_signal(&sync->sync);
}

void delegator_delegate_sync(struct delegator *d, struct delegate *dd)
{
    struct delegate_sync sync = {
        .d.run = delegator_delegate_sync_delegate,
        .real = dd,
        .sync = THREAD_SYNC_INIT,
    };
    delegator_delegate(d, &sync.d);
    thread_sync_wait(&sync.sync);
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
