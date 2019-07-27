#ifndef INCLUDE_PROTURA_LIST_H
#define INCLUDE_PROTURA_LIST_H

#include <protura/stddef.h>
#include <protura/container_of.h>

/* Very similar to the Linux-kernel list.h header (GPLv2) */

/* Doubly-linked circular linked list */

/* The design of this is such that the 'head' of a linked list, and a 'node'
 * are actually the same thing.
 *
 * The 'head' is simply an entry in the list like all the rest. When doing a
 * loop or movement that involves the head, we simply take a pointer to the
 * 'head' node in the list, and then check the rest of them against that
 * reference node.
 *
 * There are still separate 'list_node_t' and 'list_head_t' types that are
 * equivelent, for making it easier to reconize when a node is being used as
 * the head of a list, vs. being an entry in a list. The types themselves are
 * equivelent.
 */

typedef struct list_node_struct list_node_t;
typedef struct list_node_struct list_head_t;

struct list_node_struct {
    list_node_t *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
    list_head_t name = LIST_HEAD_INIT(name)

#define LIST_NODE_INIT(name) LIST_HEAD_INIT(name)
#define list_node_init(node) list_head_init(node)

static inline void list_head_init(list_head_t *head)
{
    *head = (list_head_t)LIST_HEAD_INIT(*head);
}

static inline void __list_add(list_node_t *new,
                              list_node_t *prev,
                              list_node_t *next)
{
    next->prev = new;
    new->next = next;

    new->prev = prev;
    prev->next = new;
}

static inline void list_add(list_head_t *head, list_node_t *new)
{
    __list_add(new, head, head->next);
}

static inline void list_add_tail(list_head_t *head, list_node_t *new)
{
    __list_add(new, head->prev, head);
}

#define list_attach(head, node) \
    list_add_tail(node, head)

static inline void __list_del(list_node_t *prev, list_node_t *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void __list_del_entry(list_node_t *entry)
{
    __list_del(entry->prev, entry->next);
}

/*
 * Adds the list 'list_to_add' inbetween prev and next
 */
static inline void __list_splice(list_node_t *prev, list_node_t *next, list_head_t *list_to_add)
{
    list_node_t *first = list_to_add->next;
    list_node_t *last = list_to_add->prev;

    first->prev = prev;
    prev->next = first;

    last->next = next;
    next->prev = last;
}

static inline void list_del(list_node_t *entry)
{
    __list_del_entry(entry);
    list_node_init(entry);
}

static inline void list_replace(list_node_t *new, list_node_t *old)
{
    new->next = old->next;
    new->next->prev = new;
    new->prev = old->prev;
    new->prev->next = new;
}

static inline void list_replace_init(list_node_t *new, list_node_t *old)
{
    list_replace(new, old);
    list_head_init(old);
}

static inline void list_move(list_head_t *head, list_node_t *entry)
{
    __list_del_entry(entry);
    list_add(head, entry);
}

static inline void list_move_tail(list_head_t *head, list_node_t *entry)
{
    __list_del_entry(entry);
    list_add_tail(head, entry);
}

static inline int list_is_last(const list_head_t *head, const list_node_t *entry)
{
    return entry->next == head;
}

static inline int list_empty(const list_head_t *head)
{
    return head == head->next;
}
#define list_node_is_in_list(node) !list_empty(node)

static inline void list_rotate_left(list_head_t *head)
{
    if (!list_empty(head))
        list_move_tail(head, head->next);
}

static inline void list_rotate_right(list_head_t *head)
{
    if (!list_empty(head))
        list_move(head, head->prev);
}

static inline void list_splice(list_head_t *head, list_head_t *old_list)
{
    if (!list_empty(old_list))
        __list_splice(head, head->next, old_list);
}

static inline void list_splice_tail(list_head_t *head, list_head_t *old_list)
{
    if (!list_empty(old_list))
        __list_splice(head->prev, head, old_list);
}

static inline void list_splice_init(list_head_t *head, list_head_t *old_list)
{
    if (!list_empty(old_list)) {
        __list_splice(head, head->next, old_list);
        list_head_init(old_list);
    }
}

static inline void list_splice_tail_init(list_head_t *head, list_head_t *old_list)
{
    if (!list_empty(old_list)) {
        __list_splice(head->prev, head, old_list);
        list_head_init(old_list);
    }
}


/* Moves 'first', which is already in list 'head', to the position of the first
 * entry in 'head', by rotating the list.
 *
 * The 'new_first' and 'new_last' can be thought of as doing multiple rotations
 * at once, as you could do that to achieve the same result but it would be
 * much less optimal. */
static inline void list_new_first(list_head_t *head, list_node_t *new_first)
{
    list_node_t *last = head->prev;
    list_node_t *first = head->next;
    list_node_t *new_last = new_first->prev;

    if (first == new_first)
        return ;

    /* Connect first and last list node together */
    last->next = first;
    first->prev = last;

    /* Make 'new_last' and 'new_first' the first and last nodes of the list */
    new_last->next = head;
    new_first->prev = head;

    head->prev = new_last;
    head->next = new_first;
}

static inline void list_new_last(list_head_t *head, list_node_t *new_last)
{
    list_node_t *last = head->prev;
    list_node_t *first = head->next;
    list_node_t *new_first = new_last->next;

    if (last == new_last)
        return ;

    last->next = first;
    first->prev = last;

    new_last->next = head;
    new_first->prev = head;

    head->prev = new_last;
    head->next = new_first;
}

static inline list_node_t *__list_first(list_head_t *head)
{
    return head->next;
}

#define list_first(head, type, member) \
    container_of(__list_first(head), type, member)

static inline list_node_t *__list_last(list_head_t *head)
{
    return head->prev;
}

#define list_last(head, type, member) \
    container_of(__list_last(head), type, member)

static inline list_node_t *__list_take_first(list_head_t *head)
{
    list_node_t *node = __list_first(head);
    list_del(node);
    return node;
}

#define list_take_first(head, type, member) \
    container_of(__list_take_first(head), type, member)

static inline list_node_t *__list_take_last(list_head_t *head)
{
    list_node_t *node = __list_last(head);
    list_del(node);
    return node;
}

#define list_take_last(head, type, member) \
    container_of(__list_take_last(head), type, member)


#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->prev, type, member)

#define list_first_entry_or_null(ptr, type, member) \
    (!list_empty(ptr)? list_first_entry(ptr, type, member): NULL)

#define list_next_entry(pos, member) \
    list_entry((pos)->member.next, typeof(*(pos)), member)

#define list_prev_entry(pos, member) \
    list_entry((pos)->member.prev, typeof(*(pos)), member)

#define list_foreach(head, pos) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_foreach_prev(head, pos) \
    for (pos = (head)->prev; pos != (head); pos = pos->prev)

#define list_foreach_entry(head, pos, member) \
    for (pos = list_first_entry(head, typeof(*pos), member); \
         &pos->member != (head); \
         pos = list_next_entry(pos, member))

#define list_foreach_entry_reverse(head, pos, member) \
    for (pos = list_last_entry(head, typeof(*pos), member); \
         &pos->member != (head); \
         pos = list_prev_entry(pos, member))

#define list_foreach_take_entry(head, pos, member)                                  \
    for (pos = list_empty(head)? NULL: list_take_first(head, typeof(*pos), member); \
         pos;                                                                       \
         pos = list_empty(head)? NULL: list_take_first(head, typeof(*pos), member))

#define list_foreach_take_entry_reverse(head, pos, member) \
    for (pos = list_empty(head)? NULL: list_take_last(head, typeof(*pos), member); \
         &(pos)->member != NULL;                           \
         pos = list_empty(head)? NULL: list_take_last(head, typeof(*pos), member))

#define list_ptr_is_head(head, ptr) \
    ((ptr) == (head))

#endif
