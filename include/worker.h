#ifndef OKA_WORKER_H
#define OKA_WORKER_H

#include <stdbool.h>

struct worker;

typedef bool (*worker_cancel_job_cb)(u32 type, void *data, void *opaque);
typedef void (*worker_job_cb)(struct worker *w, void *data);
typedef void (*worker_free_cb)(struct worker *w, void *data);

struct worker *worker_new(void);
void worker_free(struct worker *w);
void worker_cancel_job(struct worker *w, worker_cancel_job_cb cb, void *opaque);
void worker_add_job(struct worker *w, u32 type, worker_job_cb job_cb,
        worker_free_cb free_cb, void *data);
void worker_cancel_job_by_type(struct worker *w, u32 type);
bool worker_cancel_current(const struct worker *w);
int worker_fd(const struct worker *w);
void worker_push_result(struct worker *w, void *data);
void *worker_pop_result(struct worker *w);

#endif

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
