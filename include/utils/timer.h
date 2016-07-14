#ifndef OKA_UTILS_TIMER_H
#define OKA_UTILS_TIMER_H

#include <time.h>

#include "utils/channel.h"

struct timer;

struct timer_api {
    struct timer *(*new)(struct channel *channel, void *data);
    void (*set)(struct timer *timer, int flags, const struct itimerspec *val);
    void (*free)(struct timer *timer);
};

#endif

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
