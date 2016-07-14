#include "utils/utils.h"
#include "utils/list.h"

void list_init(struct list *head)
{
    head->next = head;
    head->prev = head;
}

void list_append(struct list *head, struct list *node)
{
    node->prev = head->prev;
    node->next = head;
    node->prev->next = node;
    node->next->prev = node;
}

void list_insert_before(struct list *before, struct list *node)
{
    list_append(before, node);
}

void list_remove(struct list *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

struct list *list_pop_first(struct list *head)
{
    auto f = head->next;
    if (f == head)
        return NULL;
    list_remove(f);
    return f;
}

bool list_empty(struct list *head)
{
    return head->next == head;
}

size_t list_len(struct list *head)
{
    size_t len = 0;
    for (auto n = head->next; n != head; n = n->next)
        len++;
    return len;
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
