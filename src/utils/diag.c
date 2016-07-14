#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils/utils.h"
#include "utils/diag.h"
#include "utils/xmalloc.h"

struct diag {
    const struct diag_api *api;
};

struct diag *diag_new(const struct diag_api *api)
{
    auto d = xnew(struct diag);
    d->api = api;
    return d;
}

void diag_free(struct diag *d)
{
    free(d);
}

__attribute__((format(printf, 1, 0))) 
static char *diag_fmt(const char *fmt, va_list ap)
{
    char *ptr = NULL;
    size_t size;

    auto f = open_memstream(&ptr, &size);
    vfprintf(f, fmt, ap);
    fclose(f);

    return ptr;
}

void diag_info(struct diag *d, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *msg = diag_fmt(fmt, ap);
    va_end(ap);
    d->api->info(msg);
}

void diag_err(struct diag *d, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *msg = diag_fmt(fmt, ap);
    va_end(ap);
    d->api->error(msg);
}

void diag_fatal(struct diag *d, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *msg = diag_fmt(fmt, ap);
    va_end(ap);
    d->api->fatal(msg);
    BUG("d->api->fatal(msg) returned");
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
