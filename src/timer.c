#include <signal.h>
#include <time.h>
#include <errno.h>

#include "utils/utils.h"
#include "utils/debug.h"
#include "utils/channel.h"
#include "utils/signals.h"
#include "utils/thread.h"
#include "utils/xmalloc.h"
#include "utils/timer.h"

#include "timer.h"

static pthread_t timer_thread;
static struct channel *timer_sync;

struct timer {
    struct channel *channel;
    void *data;
    timer_t id;
};

struct timer_sync {
    struct timer *timer;
    struct thread_sync sync;
};

static void timer_handle_expiration(struct timer *timer)
{
    channel_push(timer->channel, timer->data);
}

static void timer_signal_handler(int sig, siginfo_t *info, void *ucontext)
{
    (void)sig;
    (void)ucontext;

    timer_handle_expiration(info->si_value.sival_ptr);
}

static bool timer_remove_cb(void *data, void *opaque)
{
    return data == opaque;
}

static void *timer_loop(void *arg)
{
    (void)arg;

    sigset_t wait_set;
    sigemptyset(&wait_set);
    sigaddset(&wait_set, TIMER_SIGNAL);

    sigset_t block_set;
    sigfillset(&block_set);

    struct sigaction action = {
        .sa_sigaction = timer_signal_handler,
        .sa_flags = SA_SIGINFO,
    };
    sigfillset(&action.sa_mask);
    BUG_ON(sigaction(TIMER_SIGNAL, &action, NULL));

    struct timespec zero = { 0 };
    siginfo_t info;

    while (1) {
        thread_sigmask(SIG_UNBLOCK, &wait_set, NULL);

        auto sync = channel_pop_wait(timer_sync, struct timer_sync);

        if (!sync)
            break;

        thread_sigmask(SIG_SETMASK, &block_set, NULL);

        int rc;
        while ((rc = sigtimedwait(&wait_set, &info, &zero)) != -1)
            timer_handle_expiration(info.si_value.sival_ptr);

        BUG_ON(errno != EAGAIN);

        channel_remove(sync->timer->channel, timer_remove_cb, sync->timer->data);

        thread_sync_signal(&sync->sync);
    }

    return NULL;
}

void timer_init(void)
{
    timer_sync = channel_new(false);
    auto_restore sigs = signals_block_all();
    thread_create(&timer_thread, NULL, timer_loop, NULL);
}

void timer_exit(void)
{
    channel_push(timer_sync, NULL);
    thread_join(timer_thread, NULL);
}

static struct timer *timer_new(struct channel *channel, void *data)
{
    auto timer = xnew(struct timer);
    timer->channel = channel;
    timer->data = data;

    struct sigevent sev = {
        .sigev_notify = SIGEV_SIGNAL,
        .sigev_signo = TIMER_SIGNAL,
        .sigev_value.sival_ptr = timer,
    };
    BUG_ON(timer_create(CLOCK_REALTIME, &sev, &timer->id));

    return timer;
}

static void timer_remove_pending(struct timer *timer)
{
    struct timer_sync sync = {
        .timer = timer,
        .sync = THREAD_SYNC_INIT,
    };
    channel_push(timer_sync, &sync);
    thread_sync_wait(&sync.sync);
}

static void timer_free(struct timer *timer)
{
    BUG_ON(timer_delete(timer->id));
    timer_remove_pending(timer);
    free(timer);
}

static void timer_set(struct timer *timer, int flags, const struct itimerspec *val)
{
    BUG_ON(timer_settime(timer->id, flags, val, NULL));
}

const struct timer_api *timer_api(void)
{
    static const struct timer_api api = {
        .new = timer_new,
        .set = timer_set,
        .free = timer_free,
    };
    return &api;
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
