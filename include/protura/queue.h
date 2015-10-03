#ifndef INCLUDE_QUEUE_H
#define INCLUDE_QUEUE_H

#include <protura/stddef.h>

/* Basic Queue - A singly-linked list, but with O(1) access to the head and
 * tail of the list, for insertions at the head and tail. You can only interate
 * through the list forward, and insertions inside of the list aren't possible
 * without iterating through it. */
struct queue_node {
    struct queue_node *prev;
};

struct queue_head {
    struct queue_node *head;
    struct queue_node **tail; /* Points either to 'head', or to the 'prev'
                               * pointer of the last entry in the queue. */
};

#define QUEUE_HEAD_INIT(queue) { NULL, &(queue).head }

static inline void INIT_QUEUE_HEAD(struct queue_head *queue)
{
    queue->head = NULL;
    queue->tail = &(queue)->head;
}

static inline void queue_enqueue(struct queue_head *queue, struct queue_node *new)
{
    *queue->tail = new;
    queue->tail = &new->prev;
}

static inline struct queue_node *queue_peek(struct queue_head *queue)
{
    return queue->head;
}

static inline struct queue_node *queue_dequeue(struct queue_head *queue)
{
    struct queue_node *new = queue->head;

    if (new)
        queue->head = new->prev;

    /* Quick check if we just removed the last element in the queue. 'tail'
     * needs to point to the last element, and if the queue is empty we need to
     * point 'tail' back at 'head' so the next insertion is done at the right
     * place. */
    if (new && queue->tail == &new->prev)
        queue->tail = &queue->head;

    return new;
}

#define queue_entry(ptr, type, member) \
    container_of(ptr, type, member)

#define queue_peek_entry(queue, type, member) queue_entry(queue_peek(queue), type, member)
#define queue_dequeue_entry(queue, type, member)  queue_entry(queue_dequeue(queue), type, member)

/* Loops over every node in the queue, 'peeking' at each one */
#define queue_foreach_peek(queue, node) \
    for ((node) = (queue)->head; (node) != NULL; (node) = (node)->prev)

#define queue_foreach_peek_entry(queue, node, member) \
    for ((node) = queue_entry((queue)->head, typeof(*(node)), member); \
         (node) != NULL; \
         (node) = queue_entry((node)->member->prev, typeof(*(node)), member))

#define queue_foreach_dequeue(queue, node) \
    for (node = queue_dequeue(queue); node != NULL; node = queue_dequeue(queue))

#define queue_foreach_dequeue_entry(queue, node, member) \
    for ((node) = queue_entry(queue_dequeue(queue), typeof(*(node)), member); \
         (node) != NULL; \
         (node) = queue_entry(queue_dequeue(queue), typeof(*(node)), member))

#endif
