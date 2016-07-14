#ifndef OKA_UTILS_DEBUG_H
#define OKA_UTILS_DEBUG_H

#include <stdnoreturn.h>
#include <string.h>

__attribute__((format(printf, 2, 3)))
noreturn void debug_bug__(const char *function, const char *fmt, ...);

#define BUG(...) debug_bug__(__FUNCTION__, __VA_ARGS__)
#define BUG_ON(cond) __extension__ ({if (cond) debug_bug__(__FUNCTION__, "%s", #cond);})

#endif

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
