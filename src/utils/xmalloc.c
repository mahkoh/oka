#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "utils/utils.h"
#include "utils/xmalloc.h"
#include "utils/debug.h"

void *xmalloc__(size_t size)
{
    void *res = malloc(size);
    BUG_ON(!res);
    return res;
}

void *xrealloc__(void *ptr, size_t size)
{
    void *res = realloc(ptr, size);
    BUG_ON(!res);
    return res;
}

char *xstrdup(const char *s)
{
    char *res = strdup(s);
    BUG_ON(!res);
    return res;
}

char *xstrjoin__(struct slice slice)
{
    const char **str = slice.ptr;
    size_t i, pos = 0, len = 0;
    char *joined;
    size_t *lens;

    lens = xnew_array(size_t, slice.len);
    for (i = 0; i < slice.len; i++) {
        lens[i] = strlen(str[i]);
        len += lens[i];
    }

    joined = xnew_array(char, len + 1);
    for (i = 0; i < slice.len; i++) {
        memcpy(joined + pos, str[i], lens[i]);
        pos += lens[i];
    }
    joined[len] = 0;

    free(lens);

    return joined;
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
