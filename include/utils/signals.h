#pragma once

#include <signal.h>

#define TIMER_SIGNAL (SIGRTMIN + 0)

#define auto_restore __attribute__((cleanup(signals_restore), unused)) sigset_t

sigset_t signals_block_all(void);
void signals_restore(sigset_t *set);

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
