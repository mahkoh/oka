#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "utils/channel.h"
#include "utils/utils.h"
#include "utils/list.h"
#include "utils/xmalloc.h"
#include "utils/thread.h"
#include "utils/debug.h"

struct channel {
    struct list head;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int pipe_in;
    int pipe_out;
};

struct channel_entry {
    struct list node;
    void *data;
};

struct channel *channel_new(bool need_pipe)
{
    auto channel = xnew(struct channel);
    list_init(&channel->head);
    channel->mutex = THREAD_MUTEX_INIT;
    channel->cond = THREAD_COND_INIT;
    if (need_pipe)
        utils_init_pipe(&channel->pipe_out, &channel->pipe_in);
    else
        channel->pipe_out = channel->pipe_in = -1;
    return channel;
}

void channel_free(struct channel *c)
{
    if (c->pipe_in != -1) {
        close(c->pipe_in);
        close(c->pipe_out);
    }
    while (channel_pop(c, void))
        ;
    free(c);
}

int channel_fd(struct channel *c)
{
    BUG_ON(c->pipe_out == -1);
    return c->pipe_out;
}

void channel_clear_fd(struct channel *c)
{
    BUG_ON(c->pipe_out == -1);
    utils_clear_pipe(c->pipe_out);
}

static struct channel_entry *channel_entry_from_node(struct list *node)
{
    return container_of(node, struct channel_entry, node);
}

void channel_push(struct channel *c, void *data)
{
    auto entry = xnew(struct channel_entry);
    entry->data = data;
    auto_unlock lock = thread_mutex_lock(&c->mutex);
    list_append(&c->head, &entry->node);
    utils_signal_pipe(c->pipe_in);
    thread_cond_signal(&c->cond);
}

void *channel_pop__(struct channel *c)
{
    auto_unlock lock = thread_mutex_lock(&c->mutex);
    auto entry_node = list_pop_first(&c->head);
    if (!entry_node)
        return NULL;
    auto_free auto entry = channel_entry_from_node(entry_node);
    return entry->data;
}

void *channel_pop_wait__(struct channel *c)
{
    auto_unlock lock = thread_mutex_lock(&c->mutex);
    while (list_empty(&c->head))
        thread_cond_wait(&c->cond, &c->mutex);
    auto entry_node = list_pop_first(&c->head);
    auto_free auto entry = channel_entry_from_node(entry_node);
    return entry->data;
}

void channel_remove(struct channel *c, channel_remove_cb cb, void *opaque)
{
    auto_unlock lock = thread_mutex_lock(&c->mutex);
    struct list *node, *n;
    list_for_each_safe(node, n, &c->head) {
        auto e = channel_entry_from_node(node);
        if (cb(e->data, opaque)) {
            list_remove(node);
            free(e);
        }
    }
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
