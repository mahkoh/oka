#pragma once

#include <stdnoreturn.h>
#include <string.h>

__attribute__((format(printf, 2, 3)))
noreturn void debug_bug__(const char *function, const char *fmt, ...);

__attribute__((format(printf, 2, 3)))
void debug_warn__(const char *function, const char *fmt, ...);

#define BUG(...) debug_bug__(__FUNCTION__, __VA_ARGS__)
#define BUG_ON(cond) __extension__ ({if (cond) debug_bug__(__FUNCTION__, "%s", #cond);})

#define WARN(...) debug_warn__(__FUNCTION__, __VA_ARGS__)
#define WARN_ON(cond) __extension__ ({if (cond) debug_warn__(__FUNCTION__, "%s", #cond);})

#if 0
#   define TRACE(fmt, ...) __extension__ ({fprintf(stderr, "%s:%d: "fmt"\n", \
            __FUNCTION__, __LINE__, ##__VA_ARGS__);})
#else
#   define TRACE(fmt, ...) { }
#endif

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
