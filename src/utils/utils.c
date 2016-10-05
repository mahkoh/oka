#include <unistd.h>
#include <sys/eventfd.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "utils/utils.h"

void utils_freep(void *data)
{
    free(*(void **)data);
}

char *utils_strerr(int no)
{
    _Thread_local static char buf[128];
    if (strerror_r(no, buf, sizeof(buf)))
        strcpy(buf, "unknown error");
    return buf;
}

u64 utils_get_mono_time_ms(void)
{
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return 1000 * (u64)tp.tv_sec + (u64)tp.tv_nsec / (1000 * 1000);
}



int utils_eventfd(void)
{
    int res = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    BUG_ON(res == -1);
    return res;
}
void utils_signal_eventfd(int fd)
{
    u64 buf = 1;
    if (write(fd, &buf, sizeof(buf)) == -1)
        BUG_ON(errno != EAGAIN);
}

void utils_clear_eventfd(int fd)
{
    u64 buf;
    if (read(fd, &buf, sizeof(buf)) == -1)
        BUG_ON(errno != EAGAIN);
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
