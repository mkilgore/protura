#ifndef INCLUDE_PROTURA_EVENT_PROTOCOL_H
#define INCLUDE_PROTURA_EVENT_PROTOCOL_H

#include <uapi/protura/event/protocol.h>

enum {
    /* If set, events will be buffered even when there are no readers */
    EQUEUE_FLAG_BUFFER_EVENTS,
};

/* Currently, event-queues effectively only support one reader - if multiple
 * people open and try to read input, they'll each only get parts. */
struct event_queue {
    spinlock_t lock;

    struct wait_queue event_wait;
    int open_readers;

    /* Circular buffer of events */
    size_t head, tail, size;
    struct kern_event *buffer;

    flags_t flags;
};

#define EVENT_QUEUE_INIT(equeue, buf) \
    { \
        .lock = SPINLOCK_INIT(), \
        .event_wait = WAIT_QUEUE_INIT((equeue).event_wait), \
        .size = ARRAY_SIZE((buf)), \
        .buffer = (buf), \
    }

#define EVENT_QUEUE_INIT_FLAGS(equeue, buf, flgs) \
    { \
        .lock = SPINLOCK_INIT(), \
        .event_wait = WAIT_QUEUE_INIT((equeue).event_wait), \
        .size = ARRAY_SIZE((buf)), \
        .buffer = (buf), \
        .flags = (flgs), \
    }

void event_queue_submit_event(struct event_queue *, uint16_t type, uint16_t code, uint16_t value);

/* Wrap this function with your open call, and pass it the intended event queue */
int event_queue_open(struct file *, struct event_queue *);

int event_queue_read(struct file *, struct user_buffer, size_t);
int event_queue_release(struct file *);

/* List of minor device numbers for every event queue */
enum {
    EVENT_MINOR_KEYBOARD,
};

extern struct file_ops event_file_ops;

#endif
