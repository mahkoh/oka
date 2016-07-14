#ifndef OKA_UTILS_DIAG_H
#define OKA_UTILS_DIAG_H

#include <stdnoreturn.h>

struct diag;

struct diag_api {
    void (*error)(char *);
    void (*fatal)(char *);
    void (*info)(char *);
};

struct diag *diag_new(const struct diag_api *);
void diag_free(struct diag *d);
__attribute__((format(printf, 2, 3))) void diag_err(struct diag *, const char *fmt, ...);
__attribute__((format(printf, 2, 3))) void diag_info(struct diag *, const char *fmt, ...);
noreturn __attribute__((format(printf, 2, 3))) void diag_fatal(struct diag *,
        const char *fmt, ...);

#endif

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
