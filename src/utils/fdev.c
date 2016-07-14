#include <poll.h>
#include <errno.h>

#include "utils/fdev.h"
#include "utils/utils.h"
#include "utils/xmalloc.h"
#include "utils/list.h"

struct fdev_data {
    void *data;
    int fd;
};

struct fdev {
    struct fdev_data *data;
    struct pollfd *poll;
    size_t len;
    size_t cap;
};

struct fdev *fdev_new(void)
{
    return xnew0(struct fdev);
}

void fdev_free(struct fdev *ev)
{
    free(ev->data);
    free(ev->poll);
    free(ev);
}

static ssize_t fdev_find(struct fdev *ev, int fd)
{
    for (size_t i = 0; i < ev->len; i++) {
        if (ev->poll[i].fd == fd)
            return (ssize_t)i;
    }
    return -1;
}

void fdev_set(struct fdev *ev, int fd, void *data, short events)
{
    auto entry = fdev_find(ev, fd);

    if (entry == -1) {
        if (ev->len == ev->cap) {
            ev->cap = 1 + 2 * ev->cap;
            ev->data = xrenew(ev->data, struct fdev_data, ev->cap);
            ev->poll = xrenew(ev->poll, struct pollfd, ev->cap);
        }
        entry = (ssize_t)ev->len++;
    }

    ev->data[entry].fd = fd;
    ev->data[entry].data = data;
    ev->poll[entry].fd = fd;
    ev->poll[entry].events = events;
}

void fdev_disable(struct fdev *ev, int fd)
{
    auto entry = fdev_find(ev, fd);
    BUG_ON(entry == -1);
    ev->poll[entry].fd = -1;
}

void *fdev_get_opaque(struct fdev *ev, int fd)
{
    auto entry = fdev_find(ev, fd);
    if (entry == -1)
        return NULL;
    return ev->data[entry].data;
}

void fdev_remove(struct fdev *ev, int fd)
{
    auto entry = fdev_find(ev, fd);
    if (entry == -1)
        return;
    memcpy(ev->data + entry, ev->data + entry + 1,
            sizeof(*ev->data) * (ev->len - (size_t)entry - 1));
    memcpy(ev->poll + entry, ev->poll + entry + 1,
            sizeof(*ev->poll) * (ev->len - (size_t)entry - 1));
    ev->len--;
}

ssize_t fdev_wait(struct fdev *ev, struct fdev_event *events, size_t n, int timeout)
{
    int res = poll(ev->poll, ev->len, timeout);
    if (res == -1)
        return -errno;

    size_t pos = 0;
    for (size_t i = 0; i < ev->len && pos < n; i++) {
        if (ev->poll[i].revents != 0) {
            events[pos].events = ev->poll[i].revents;
            events[pos].fd = ev->poll[i].fd;
            events[pos].data = ev->data[i].data;
            pos++;
        }
    }
    return (ssize_t)pos;
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
