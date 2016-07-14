#ifndef OKA_UTILS_THREAD_H
#define OKA_UTILS_THREAD_H

#include <pthread.h>
#include <stdbool.h>

struct thread_sync {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool done;
};

#define THREAD_SYNC_INIT (struct thread_sync) { \
    .mutex = THREAD_MUTEX_INIT, \
    .cond = THREAD_COND_INIT, \
    .done = false, \
}
#define THREAD_MUTEX_INIT (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER
#define THREAD_COND_INIT (pthread_cond_t)PTHREAD_COND_INITIALIZER
#define auto_unlock \
    __attribute__((cleanup(thread_mutex_unlockp), unused)) pthread_mutex_t * 

void thread_create(pthread_t *thread, const pthread_attr_t *attr,
        void *(*start_routine)(void *), void *arg);
void thread_join(pthread_t thread, void **retval);

pthread_mutex_t *thread_mutex_lock(pthread_mutex_t *mutex);
void thread_mutex_unlock(pthread_mutex_t *mutex);
void thread_mutex_unlockp(pthread_mutex_t **mutex);

void thread_cond_signal(pthread_cond_t *cond);
void thread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);

void thread_once(pthread_once_t *once_control, void (*init_routine)(void));

void thread_sigmask(int how, const sigset_t *set, sigset_t *oldset);

void thread_sync_wait(struct thread_sync *s);
void thread_sync_signal(struct thread_sync *s);

#endif

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
