#ifndef OKA_UTILS_VEC_H
#define OKA_UTILS_VEC_H

#include <stdlib.h>

#include "utils/xmalloc.h"

#define UTILS_VECTOR(name, type) \
    struct name##_vector { \
        type *ptr; \
        size_t len; \
        size_t cap; \
    }; \
    __attribute__((unused)) \
    static void name##_vector_push(struct name##_vector *v, type t) \
    { \
        if (v->len == v->cap) { \
            v->cap = 2 * v->cap + 1; \
            v->ptr = xrenew(v->ptr, type, sizeof(type) * v->cap); \
        } \
        v->ptr[v->len++] = t; \
    } \
    __attribute__((unused)) \
    static void name##_vector_remove(struct name##_vector *v, size_t idx) \
    { \
        v->len--; \
        memmove(v->ptr + idx, v->ptr + idx + 1, (v->len - idx) * sizeof(type)); \
    } \
    __attribute__((unused)) \
    static void name##_vector_swap_remove(struct name##_vector *v, size_t idx) \
    { \
        if (idx < v->len - 1) \
            v->ptr[idx] = v->ptr[v->len - 1]; \
        v->len--; \
    } \

#endif
