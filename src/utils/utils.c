#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

#include "utils/utils.h"

void utils_freep(void *data)
{
    free(*(void **)data);
}

void utils_signal_pipe(int pipe)
{
    u8 buf = 0;
    write(pipe, &buf, 1);
}

void utils_clear_pipe(int pipe)
{
    u8 buf[128];
    read(pipe, buf, sizeof(buf));
}

void utils_init_pipe(int *pipe_out, int *pipe_in)
{
    int buf[2];
    BUG_ON(pipe(buf));
    int fl = fcntl(buf[0], F_GETFL);
    BUG_ON(fl == -1);
    fl = fcntl(buf[0], F_SETFL, fl | O_NONBLOCK);
    BUG_ON(fl == -1);
    *pipe_out = buf[0];
    *pipe_in = buf[1];
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

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
