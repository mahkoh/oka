#include <pthread.h>
#include <stdio.h>
#include <stdatomic.h>

#include "utils/utils.h"
#include "utils/channel.h"
#include "utils/xmalloc.h"
#include "utils/thread.h"
#include "utils/signals.h"

#include "worker.h"

struct worker_job {
    u32 type;
    worker_job_cb job_cb;
    worker_free_cb free_cb;
    void *data;
};

struct worker {
    pthread_t thread;
    struct channel *jobs;
    struct channel *results;
    struct worker_job *current;
    atomic_bool cancel_current;
    pthread_mutex_t mutex;
};

struct worker_cancel_data {
    struct worker *worker;
    worker_cancel_job_cb cb;
    void *opaque;
};

static void worker_unlock(struct worker *w, pthread_mutex_t **mutex)
{
    thread_mutex_unlock(&w->mutex);
    *mutex = NULL;
}

static pthread_mutex_t *worker_lock(struct worker *w)
{
    return thread_mutex_lock(&w->mutex);
}

static void *worker_loop(void *arg)
{
    struct worker *w = arg;
    while (1) {
        auto_free auto job = channel_pop_wait(w->jobs, struct worker_job);
        if (!job)
            break;

        auto_unlock lock = worker_lock(w);
        w->cancel_current = false;
        w->current = job;
        worker_unlock(w, &lock);

        job->job_cb(w, job->data);

        lock = worker_lock(w);
        job->free_cb(w, job->data);
        w->current = NULL;

        (void)lock;
    }
    return NULL;
}

struct worker *worker_new(void)
{
    auto_restore sigs = signals_block_all();

    auto worker = xnew(struct worker);
    worker->jobs = channel_new(false);
    worker->results = channel_new(true);
    worker->current = NULL;
    worker->cancel_current = false;
    worker->mutex = THREAD_MUTEX_INIT;
    thread_create(&worker->thread, NULL, worker_loop, worker);
    return worker;
}

static bool worker_cancel_job_channel_cb(void *data, void *opaque)
{
    struct worker_cancel_data *cd = opaque;
    struct worker_job *job = data;
    auto ret = cd->cb(job->type, job->data, cd->opaque);
    if (ret) {
        job->free_cb(cd->worker, job->data);
        free(job);
    }
    return ret;
}

void worker_push_result(struct worker *w, void *data)
{
    if (data)
        channel_push(w->results, data);
}

bool worker_cancel_current(const struct worker *w)
{
    return w->cancel_current;
}

static bool worker_cancel_job_by_type_cb(u32 type, void *data, void *opaque)
{
    (void)data;
    u32 *cancel_type = opaque;
    return (type & *cancel_type) != 0;
}

void worker_cancel_job_by_type(struct worker *w, u32 type)
{
    worker_cancel_job(w, worker_cancel_job_by_type_cb, &type);
}

void worker_cancel_job(struct worker *w, worker_cancel_job_cb cb, void *opaque)
{
    struct worker_cancel_data data = {
        .worker = w,
        .cb = cb,
        .opaque = opaque,
    };
    channel_remove(w->jobs, worker_cancel_job_channel_cb, &data);

    auto_unlock lock = worker_lock(w);
    auto cur = w->current;
    if (cur && cb(cur->type, cur->data, opaque))
        w->cancel_current = true;
}

void worker_add_job(struct worker *w, u32 type, worker_job_cb job_cb,
        worker_free_cb free_cb, void *data)
{
    auto job = xnew(struct worker_job);
    job->type = type;
    job->job_cb = job_cb;
    job->free_cb = free_cb;
    job->data = data;
    channel_push(w->jobs, job);
}

void worker_free(struct worker *w)
{
    worker_cancel_job_by_type(w, (u32)-1);
    channel_push(w->jobs, NULL);
    thread_join(w->thread, NULL);
    channel_free(w->jobs);
    channel_free(w->results);
}

int worker_fd(const struct worker *w)
{
    return channel_fd(w->results);
}

void *worker_pop_result(struct worker *w)
{
    return channel_pop(w->results, void);
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
