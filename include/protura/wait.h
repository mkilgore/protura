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
#include <protura/time.h>
#include <arch/spinlock.h>
#include <protura/waitbits.h>
#include <protura/signal.h>
#include <protura/work.h>

/* Wait queues are used when waiting for 'events' to happen. When the event
 * happens, all of the `struct work` entries in the queue are scheduled.
 *
 * Thus, the general way to use this is via a register/check/sleep loop, which is
 * conviently wrapped up into the 'wait_event' macros below.
 *
 * When modifying the queue, 'lock' has to be held. */
struct wait_queue {
    list_head_t queue;
    spinlock_t lock;
};

struct wait_queue_node {
    list_node_t node;
    struct wait_queue *queue;

    struct work on_complete;
};

#define WAIT_QUEUE_INIT(q, name) \
    { .queue = LIST_HEAD_INIT((q).queue), \
      .lock = SPINLOCK_INIT(name) }

#define WAIT_QUEUE_NODE_INIT(q) \
    { .node = LIST_NODE_INIT((q).node), \
      .on_complete = WORK_INIT((q).on_complete), \
      .queue = NULL }

void wait_queue_init(struct wait_queue *);
void wait_queue_node_init(struct wait_queue_node *);

/* Register or unregister the current task to wakeup from this wait-queue. */
void wait_queue_register(struct wait_queue *, struct wait_queue_node *);
void wait_queue_unregister(struct wait_queue_node *);

/* Called by the task that is done with whatever the tasks waiting in the queue
 * are waiting for. */
int wait_queue_wake(struct wait_queue *);

pid_t sys_waitpid(pid_t pid, int *wstatus, int options);

pid_t sys_wait(int *ret);

#define WNOHANG      __kWNOHANG
#define WUNTRACED    __kWUNTRACED
#define WSTOPPED     __kWSTOPPED
#define WCONTINUED   __kWCONTINUED

#define WIFEXITED    __kWIFEXITED
#define WIFSIGNALED  __kWIFSIGNALED
#define WIFSTOPPED   __kWIFSTOPPED

#define WEXITSTATUS  __kWEXITSTATUS
#define WTERMSIG     __kWTERMSIG
#define WSTOPSIG     __kWSTOPSIG

#define WIFCONTINUED __kWIFCONTINUED

#define WCONTINUED_MAKE() (__kWCONTINUEDBITS)
#define WEXIT_MAKE(ret) (((ret) << 8) | 0)
#define WSIGNALED_MAKE(sig) (sig)
#define WSTOPPED_MAKE(sig) (((sig) << 8) | 0x7F)

#define __wait_queue_event_generic(queue, condition, is_intr, cmd1, cmd2) \
    ({ \
        int ____ret = 0; \
        while (1) { \
            int sig_is_pending = is_intr? has_pending_signal(cpu_get_local()->current): 0; \
            \
            wait_queue_register(queue, &cpu_get_local()->current->wait); \
            if (is_intr) \
                scheduler_set_intr_sleeping(); \
            else \
                scheduler_set_sleeping(); \
            \
            if (condition) \
                break; \
            \
            if (sig_is_pending) { \
                ____ret = -ERESTARTSYS; \
                break; \
            } \
            \
            cmd1; \
            \
            scheduler_task_yield(); \
            \
            cmd2; \
        } \
        wait_queue_unregister(&cpu_get_local()->current->wait); \
        scheduler_set_running(); \
        ____ret; \
    })

#define wait_queue_event_generic(queue, condition, is_intr, cmd1, cmd2) \
    ({ \
        int __ret = 0; \
        if (!(condition)) \
            __ret = __wait_queue_event_generic(queue, condition, is_intr, cmd1, cmd2); \
        __ret; \
    })

/*
 * Wrappers around wait-queue functionality.
 */
#define wait_queue_event(queue, condition) \
    wait_queue_event_generic(queue, condition, 0, do { ; } while (0), do { ; } while (0))

#define wait_queue_event_intr(queue, condition) \
    wait_queue_event_generic(queue, condition, 1, do { ; } while (0), do { ; } while (0))

#define wait_queue_event_intr_cmd(queue, condition, cmd1, cmd2) \
    wait_queue_event_generic(queue, condition, 1, cmd1, cmd2)

#define wait_queue_event_spinlock(queue, condition, lock) \
    wait_queue_event_generic(queue, condition, 0, spinlock_release(lock), spinlock_acquire(lock))

#define wait_queue_event_intr_spinlock(queue, condition, lock) \
    wait_queue_event_generic(queue, condition, 1, spinlock_release(lock), spinlock_acquire(lock))

#define wait_queue_event_mutex(queue, condition, lock) \
    wait_queue_event_generic(queue, condition, 0, mutex_unlock(lock), mutex_lock(lock))

#define wait_queue_event_intr_mutex(queue, condition, lock) \
    wait_queue_event_generic(queue, condition, 1, mutex_unlock(lock), mutex_lock(lock))

#endif
