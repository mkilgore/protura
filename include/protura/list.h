#ifndef INCLUDE_PROTURA_LIST_H
#define INCLUDE_PROTURA_LIST_H

/* Very similar to the Linux-kernel list.h header (GPLv2) */

struct list_node {
    struct list_node *next, *prev;
};

struct list_head {
    struct list_node n;
};

#define LIST_HEAD_INIT(name) { { &(name).n, &(name).n } }

#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->n.next = &list->n;
    list->n.prev = &list->n;
}

#define LIST_POISON1 ((void *)0x00001010)
#define LIST_POISON2 ((void *)0x00002020)

static inline void __list_add(struct list_node *new,
                              struct list_node *prev,
                              struct list_node *next)
{
    next->prev = new;
    new->next = next;

    new->prev = prev;
    prev->next = new;
}

static inline void list_add(struct list_head *head, struct list_node *new)
{
    __list_add(new, &head->n, head->n.next);
}

static inline void list_add_tail(struct list_head *head, struct list_node *new)
{
    __list_add(new, head->n.prev, &head->n);
}

static inline void __list_del(struct list_node *prev, struct list_node *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void __list_del_entry(struct list_node *entry)
{
    __list_del(entry->prev, entry->next);
}

static inline void list_del(struct list_node *entry)
{
    __list_del_entry(entry);
    entry->next = LIST_POISON1;
    entry->prev = LIST_POISON2;
}

static inline void list_replace(struct list_node *new, struct list_node *old)
{
    new->next = old->next;
    new->next->prev = new;
    new->prev = old->prev;
    new->prev->next = new;
}

static inline void list_move(struct list_head *head, struct list_node *entry)
{
    __list_del_entry(entry);
    list_add(head, entry);
}

static inline void list_move_tail(struct list_head *head, struct list_node *entry)
{
    __list_del_entry(entry);
    list_add_tail(head, entry);
}

static inline int list_is_last(const struct list_head *head, const struct list_node *entry)
{
    return entry->next == &head->n;
}

static inline int list_empty(const struct list_head *head)
{
    return &head->n == head->n.next;
}

static inline void list_rotate_left(struct list_head *head)
{
    if (!list_empty(head))
        list_move_tail(head, head->n.next);
}

static inline void list_rotate_right(struct list_head *head)
{
    if (!list_empty(head))
        list_move(head, head->n.prev);
}

/* Moves 'first', which is already in list 'head', to the position of the first
 * entry in 'head', by rotating the list. */
static inline void list_new_first(struct list_head *head, struct list_node *new_first)
{
    struct list_node *last = head->n.prev;
    struct list_node *first = head->n.next;
    struct list_node *new_last = new_first->prev;

    if (first == new_first)
        return ;

    /* Connect first and last list node together */
    last->next = first;
    first->prev = last;

    /* Make 'new_last' and 'new_first' the first and last nodes of the list */
    new_last->next = &head->n;
    new_first->prev = &head->n;

    head->n.prev = new_last;
    head->n.next = new_first;
}

static inline void list_new_last(struct list_head *head, struct list_node *new_last)
{
    struct list_node *last = head->n.prev;
    struct list_node *first = head->n.next;
    struct list_node *new_first = new_last->next;

    if (last == new_last)
        return ;

    last->next = first;
    first->prev = last;

    new_last->next = &head->n;
    new_first->prev = &head->n;

    head->n.prev = new_last;
    head->n.next = new_first;
}

static inline struct list_node *__list_first(struct list_head *head)
{
    return head->n.next;
}

#define list_first(head, type, member) \
    container_of(__list_first(head), type, member)

static inline struct list_node *__list_last(struct list_head *head)
{
    return head->n.prev;
}

#define list_last(head, type, member) \
    container_of(__list_last(head), type, member)

static inline struct list_node *__list_take_first(struct list_head *head)
{
    struct list_node *node = __list_first(head);
    list_del(node);
    return node;
}

#define list_take_first(head, type, member) \
    container_of(__list_take_first(head), type, member)

static inline struct list_node *__list_take_last(struct list_head *head)
{
    struct list_node *node = __list_last(head);
    list_del(node);
    return node;
}

#define list_take_last(head, type, member) \
    container_of(__list_take_last(head), type, member)


#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->n.next, type, member)

#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->n.prev, type, member)

#define list_first_entry_or_null(ptr, type, member) \
    (!list_empty(ptr)? list_first_entry(ptr, type, member): NULL)

#define list_next_entry(pos, member) \
    list_entry((pos)->member.next, typeof(*(pos)), member)

#define list_prev_entry(pos, member) \
    list_entry((pos)->member.prev, typeof(*(pos)), member)

#define list_foreach(head, pos) \
    for (pos = (head)->n.next; pos != &(head)->n; pos = pos->next)

#define list_foreach_prev(head, pos) \
    for (pos = (head)->n.prev; pos != &(head)->n; pos = pos->prev)

#define list_foreach_entry(head, pos, member) \
    for (pos = list_first_entry(head, typeof(*pos), member); \
         &pos->member != &(head)->n; \
         pos = list_next_entry(pos, member))

#define list_foreach_entry_reverse(head, pos, member) \
    for (pos = list_last_entry(head, typeof(*pos), member); \
         &pos->member != &(head)->n; \
         pos = list_prev_entry(pos, member))

#define list_ptr_is_head(head, ptr) \
    ((ptr) == (&(head)->n))
#endif
