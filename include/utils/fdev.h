#ifndef OKA_UTILS_FDEV_H
#define OKA_UTILS_FDEV_H

#include <unistd.h>

struct fdev;

struct fdev_event {
    void *data;
    int fd;
    short events;
};

struct fdev *fdev_new(void);
void fdev_free(struct fdev *ev);
void fdev_set(struct fdev *ev, int fd, void *data, short events);
void *fdev_get_opaque(struct fdev *ev, int fd);
void fdev_disable(struct fdev *ev, int fd);
void fdev_remove(struct fdev *ev, int fd);
ssize_t fdev_wait(struct fdev *ev, struct fdev_event *events, size_t n, int timeout);

#endif

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
