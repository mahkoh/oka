#include <pthread.h>
#include <signal.h>

#include "utils/utils.h"
#include "utils/thread.h"
#include "utils/debug.h"

#define thread_test(fn) __extension__ ({\
    int rc = fn; \
    if (rc) \
        BUG(#fn ": %s", utils_strerr(rc)); \
})

void thread_create(pthread_t *thread, const pthread_attr_t *attr,
        void *(*start_routine)(void *), void *arg)
{
    thread_test(pthread_create(thread, attr, start_routine, arg));
}

void thread_join(pthread_t thread, void **retval)
{
    thread_test(pthread_join(thread, retval));
}

pthread_mutex_t *thread_mutex_lock(pthread_mutex_t *mutex)
{
    thread_test(pthread_mutex_lock(mutex));
    return mutex;
}

void thread_mutex_unlock(pthread_mutex_t *mutex)
{
    thread_test(pthread_mutex_unlock(mutex));
}

void thread_mutex_unlockp(pthread_mutex_t **mutex)
{
    if (*mutex)
        thread_mutex_unlock(*mutex);
}

void thread_cond_signal(pthread_cond_t *cond)
{
    thread_test(pthread_cond_signal(cond));
}

void thread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    thread_test(pthread_cond_wait(cond, mutex));
}

void thread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
    thread_test(pthread_once(once_control, init_routine));
}

void thread_sigmask(int how, const sigset_t *set, sigset_t *oldset)
{
    thread_test(pthread_sigmask(how, set, oldset));
}

void thread_sync_wait(struct thread_sync *s)
{
    auto_unlock lock = thread_mutex_lock(&s->mutex);
    while (!s->done)
        thread_cond_wait(&s->cond, &s->mutex);
    s->done = false;
}

void thread_sync_signal(struct thread_sync *s)
{
    auto_unlock lock = thread_mutex_lock(&s->mutex);
    s->done = true;
    thread_cond_signal(&s->cond);
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
