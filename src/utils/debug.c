#include <stdnoreturn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils/debug.h"

__attribute__((format(printf, 2, 3)))
noreturn void debug_bug__(const char *function, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "\n%s: BUG: ", function);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");

    abort();
}

__attribute__((format(printf, 2, 3)))
void debug_warn__(const char *function, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "\n%s: WARNIG: ", function);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
