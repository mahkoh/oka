#ifndef OKA_UTILS_XMALLOC_H
#define OKA_UTILS_XMALLOC_H

#include "utils/utils.h"

#define xnew_uninit(type) __extension__ ((typeof(type) *)xmalloc__(sizeof(type)))
#define xnew_array(type, len) __extension__ \
    ((typeof(type) *)xmalloc__(sizeof(type) * (len)))
#define xnew0(type) __extension__ ({ \
    typeof(type) *p = xmalloc__(sizeof(type)); \
    *p = (typeof(type)){ 0 }; \
    p; \
})
#define xrenew(ptr, type, num) __extension__ \
    ((typeof(type) *)xrealloc__(ptr, sizeof(type) * num))
#define xstrjoin(...) xstrjoin__(TO_SLICE(const char *, __VA_ARGS__))

void *xmalloc__(size_t size);
void *xrealloc__(void *ptr, size_t size);
char *xstrdup(const char *s);
char *xstrjoin__(struct slice slice);

#endif

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
