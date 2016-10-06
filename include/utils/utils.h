#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "utils/debug.h"

#define auto __auto_type

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define PRINTF(a, b) __attribute__((format(printf, a, b)))

#define offset_of(type, member) __builtin_offsetof(type, member)

#define types_compatible(a, b) __builtin_types_compatible_p(a, b)

#define discard_const(val, type) __extension__ ({ \
    static_assert(types_compatible(typeof(*val), type), "incompatible types"); \
    (type *)(uintptr_t)val; \
})

#define container_of(ptr, type, member) __extension__ ({ \
    typedef typeof(*ptr) val_type; \
    typedef typeof(((type *)0)->member) member_type; \
    static_assert(types_compatible(val_type, member_type), "incompatible types"); \
    (type *)((uintptr_t)(ptr) - offset_of(type, member)); \
})

#define max(a, b) __extension__ ({ \
    auto _a = a; \
    auto _b = b; \
    _a > _b ? _a : _b; \
})

#define min(a, b) __extension__ ({ \
    auto _a = a; \
    auto _b = b; \
    _a > _b ? _b : _a; \
})

#define move(a) __extension__ ({ \
    auto tmp = a; \
    a = NULL; \
    tmp; \
})

#define auto_free __attribute__((cleanup(utils_freep), unused))

#define PACKED __attribute__((packed))

#define utils_bswap(x) _Generic((x), \
    char: (x), \
    unsigned char: (x), \
    signed char: (x), \
    u16: ((u16)__builtin_bswap16((u16)x)), \
    i16: ((i16)__builtin_bswap16((u16)x)), \
    u32: ((u32)__builtin_bswap32((u32)x)), \
    i32: ((i32)__builtin_bswap32((u32)x)), \
    u64: ((u64)__builtin_bswap64((u64)x)), \
    i64: ((i64)__builtin_bswap64((u64)x)) \
)

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
# define utils_from_le(x) utils_bswap(x)
# define utils_from_be(x) (x)
#else
# define utils_from_le(x) (x)
# define utils_from_be(x) utils_bswap(x)
#endif

#define utils_leading_zeros(x) ((size_t)_Generic((x), \
    char: __builtin_clz((unsigned int)(unsigned char)(x)) - 8 * (sizeof(int) - 1), \
    unsigned char: __builtin_clz((unsigned int)(x)) - 8 * (sizeof(int) - 1), \
    signed char: __builtin_clz((unsigned int)(unsigned char)(x)) - 8 * (sizeof(int) - 1), \
    short: __builtin_clz((unsigned int)(unsigned short)(x)) - 8 * (sizeof(int) - sizeof(short)), \
    unsigned short: __builtin_clz((unsigned int)(x)) - 8 * (sizeof(int) - sizeof(short)), \
    int: __builtin_clz((unsigned int)(x)), \
    unsigned int: __builtin_clz((x)), \
    long: __builtin_clzl((unsigned long)(x)), \
    unsigned long: __builtin_clzl((x)), \
    long long: __builtin_clzll((unsigned long long)(x)), \
    unsigned long long: __builtin_clzll((x)) \
))

#define utils_next_power_of_two(x) __extension__ ({ \
    auto tmp = (x); \
    const auto width = 8 * sizeof(tmp); \
    typedef typeof(x) type; \
    (type)((type)1 << ((width - utils_leading_zeros(tmp - 1)) % width)); \
})

#define utils_utf8_width(x) \
    ((size_t)__builtin_clz(~(unsigned int)(int)(i8)x) - 8 * (sizeof(int) - 1))

#define N_ELEMENTS(array) (sizeof(array) / sizeof((array)[0]))

struct slice {
    void *ptr;
    size_t len;
};

#define TO_SLICE(type, ...) ((struct slice) { \
    .ptr = ((type[]){__VA_ARGS__}), \
    .len = N_ELEMENTS(((type[]){__VA_ARGS__})), \
})

char *utils_strerr(int no);
void utils_freep(void *data);
u64 utils_get_mono_time_ms(void);

int utils_eventfd(void);
void utils_signal_eventfd(int fd);
void utils_clear_eventfd(int fd);

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
