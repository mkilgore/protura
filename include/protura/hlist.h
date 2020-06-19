#ifndef INCLUDE_PROTURA_HLIST_H
#define INCLUDE_PROTURA_HLIST_H

#include <protura/stddef.h>
#include <protura/list.h>

struct hlist_node {
    struct hlist_node **pprev, *next;
};

struct hlist_head {
    struct hlist_node *first;
};

typedef struct hlist_node hlist_node_t;
typedef struct hlist_head hlist_head_t;

#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) hlist_head_t name = HLIST_HEAD_INIT(name)

#define HLIST_NODE_INIT() { 0 }

static inline void hlist_node_init(hlist_node_t *n)
{
    n->next = NULL;
    n->pprev = NULL;
}

static inline int hlist_empty(const hlist_head_t *h)
{
    return !h->first;
}

static inline void __hlist_del(hlist_node_t *n)
{
    hlist_node_t *next = n->next;
    hlist_node_t **pprev = n->pprev;
    *pprev = next;
    if (next)
        next->pprev = pprev;
}

static inline int hlist_hashed(hlist_node_t *n)
{
    return !!n->pprev;
}

static inline void hlist_del(hlist_node_t *n)
{
    if (hlist_hashed(n)) {
        __hlist_del(n);
        n->next = NULL;
        n->pprev = NULL;
    }
}

static inline void hlist_add(hlist_head_t *head, hlist_node_t *n)
{
    hlist_node_t *first = head->first;
    n->next = first;
    if (first)
        first->pprev = &n->next;
    head->first = n;
    n->pprev = &head->first;
}

#define hlist_entry(head, type, member) \
    container_of(head, type, member)

/* Necessary because container_of will modify the 'head' pointer, with the
 * result being that if it starts out as NULL, it won't end as NULL */
#define hlist_entry_or_null(head, type, member) \
    ({ \
        typeof(head) __hlist_tmp = (head); \
        ((__hlist_tmp)? hlist_entry(__hlist_tmp, type, member): NULL); \
    })

#define hlist_foreach(head, pos) \
    for (pos = (head)->first; pos; pos = (pos)->next)

#define hlist_foreach_entry(head, pos, member) \
    for (pos = hlist_entry_or_null((head)->first, typeof(*(pos)), member); \
         pos; \
         pos = hlist_entry_or_null((pos)->member.next, typeof(*(pos)), member))

#endif
