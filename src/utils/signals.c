#include <signal.h>
#include <stddef.h>

#include "utils/signals.h"
#include "utils/thread.h"

sigset_t signals_block_all(void)
{
    sigset_t oldset, set;
    sigfillset(&set);
    thread_sigmask(SIG_BLOCK, &set, &oldset);
    return oldset;
}

void signals_restore(sigset_t *set)
{
    thread_sigmask(SIG_SETMASK, set, NULL);
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
