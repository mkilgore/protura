/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_WAIT_H
#define INCLUDE_PROTURA_WAIT_H

#include <protura/list.h>
#include <arch/spinlock.h>

struct task;


#define WAKEUP_LIST_MAX_TASKS 20

/* Wakeup lists are for registering multiple tasks to be woke-up on various
 * types of events. For example, waiting until a keypress happens. These events
 * are typically things that multiple tasks will want to be notified of at the
 * same time, and the tasks will want to be notified for every event that
 * happens. */
struct wakeup_list {
    struct task *tasks[WAKEUP_LIST_MAX_TASKS];
};

void wakeup_list_init(struct wakeup_list *);
void wakeup_list_add(struct wakeup_list *, struct task *);
void wakeup_list_remove(struct wakeup_list *, struct task *);
void wakeup_list_wakeup(struct wakeup_list *);

/* Wakeup queues are for when multiple tasks need to be woke-up in series of
 * when they ask. For example, when wake-up queues can be used when multiple
 * tasks need access to a buffer. Each task asks for the buffer, and then gets
 * put in a wakeup_queue. When the current task using the buffer releases it,
 * the next task in the queue will be woke-up and can then get access to the
 * buffer.
 *
 * When modifying the queue, 'lock' has to be held. */
struct wait_queue {
    struct list_head queue;
    spinlock_t lock;
};

struct wait_queue_node {
    struct list_node node;
    struct wait_queue *queue;
};

#define WAIT_QUEUE_INIT(q, name) \
    { .queue = LIST_HEAD_INIT((q).queue), \
      .lock = SPINLOCK_INIT(name) }

void wait_queue_init(struct wait_queue *);

/* Register or unregister the current task to wakeup from this wait-queue. */
void wait_queue_register(struct wait_queue *);
void wait_queue_unregister(void);

/* Called by the task that is done with whatever the tasks waiting in the queue are waiting for */
void wait_queue_wake(struct wait_queue *);
void wait_queue_wake_all(struct wait_queue *);


/* For explinations of the below macros, see the 'sleep' macro in scheduler.h */

#define using_wait_queue(queue) \
    using_nocheck(wait_queue_register(queue), wait_queue_unregister(queue))

#define sleep_with_wait_queue_begin(queue) \
    (wait_queue_register(queue), scheduler_set_sleeping())

#define sleep_with_wait_queue_end() \
    (scheduler_set_running(), wait_queue_unregister())

/* While it would be nice if we could just combine the 'using_wait_queue' and
 * 'sleep' macros here, we can only have one 'using' macro per line, so we have
 * to settle for one 'using' macro that does both of the above. */
#define sleep_with_wait_queue(queue) \
    using_nocheck(sleep_with_wait_queue_begin(queue), \
            (sleep_with_wait_queue_end()))

#endif
