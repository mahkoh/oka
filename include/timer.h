#ifndef OKA_TIMER_H
#define OKA_TIMER_H

#include "utils/timer.h"

void timer_init(void);
void timer_exit(void);
const struct timer_api *timer_api(void);

#endif

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
