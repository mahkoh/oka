#ifndef OKA_UTILS_LIST_H
#define OKA_UTILS_LIST_H

#include <stdbool.h>

struct list {
    struct list *prev;
    struct list *next;
};

void list_init(struct list *head);
void list_append(struct list *head, struct list *node);
void list_insert_before(struct list *before, struct list *node);
void list_remove(struct list *node);
struct list *list_pop_first(struct list *head);
bool list_empty(struct list *head);
size_t list_len(struct list *head);

#define list_for_each_safe(node, n, head) \
    for (node = (head)->next, n = node->next; node != (head); node = n, n = n->next)

#define list_for_each_entry(entry, head, member) \
    for (entry = NULL; __extension__ ({ \
        auto node = entry ? entry->member.next : (head)->next; \
        if (node != (head)) entry = container_of(node, typeof(*entry), member); \
        node != (head); \
        });)

#endif

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
