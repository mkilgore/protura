/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/list.h>
#include <protura/string.h>
#include <arch/spinlock.h>
#include <protura/atomic.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/palloc.h>
#include <protura/mm/user_ptr.h>
#include <protura/task.h>

#include <protura/fs/file.h>
#include <protura/fs/poll.h>

/*
 * The poll_table situation is largely opaque to the outside world, only
 * offering a 'poll_table_add' function to add a wait_queue. The implementation
 * of it is defined in this file.
 *
 * Races/issues to keep in mind:
 *   Wake-ups can be recieved any time after we register for a wait-queue.
 *     - We also have to be registered for a wait-queue before we can
 *       accurately check a condition variable.
 *
 *   It is impossible to accurately 'catch' wake-ups across anything that may
 *     sleep, like locking a mutex. There is no indication of "where" a wake-up
 *     came from, so the wake-up will be lost if you're waiting in the mutex's
 *     wait-queue and recieve a different wait-queue's wake. The mutex code will
 *     just skip it, as will any other sleeping code. This is a problem since
 *     poll() functions may need to take a mutex.
 *
 * The biggest issues are due to the fact that the normal wait_queue
 * notifications are not sufficent for our needs here. The task state can
 * change in undesirable situations, and the wake-up's relaly can't be
 * accurately detected.
 *
 * The fix is to not sleep while calling the various poll() callbacks, and
 * instead use a callback in the wait_queue. The wait_queue's associated with a
 * poll() call are all given a callback that sets the 'event' variable in the
 * associated poll_table. In poll(), after calling the various poll() calls on
 * the fd's, we sleep and check 'event'.
 *
 * 'event' will only be set by one of our wait_queues so we use it as an
 * indication we were "woken-up" by a wait_queue and shouldn't sleep.
 *
 * If it is not set after we start sleeping, then it is clear we have not yet
 * been woken-up by one of our wait_queues and can safely suspend ourselves
 * until we recieve a wake-up.
 */

struct poll_table_entry {
    struct wait_queue_node wait_node;
    struct poll_table *table;

    list_node_t poll_table_node;
};

struct poll_table {
    int event;
    list_head_t entries;
};

#define POLL_TABLE_ENTRY_INIT(entry) \
    { \
        .wait_node = WAIT_QUEUE_NODE_INIT((entry).wait_node), \
        .poll_table_node = LIST_NODE_INIT((entry).poll_table_node), \
    }

#define POLL_TABLE_INIT(table) \
    { \
        .entries = LIST_HEAD_INIT((table).entries), \
    }

static inline void poll_table_entry_init(struct poll_table_entry *entry)
{
    *entry = (struct poll_table_entry)POLL_TABLE_ENTRY_INIT(*entry);
}

static inline void poll_table_init(struct poll_table *table)
{
    *table = (struct poll_table)POLL_TABLE_INIT(*table);
}

static void poll_table_wait_queue_callback(struct wait_queue_node *node)
{
    struct poll_table_entry *entry = container_of(node, struct poll_table_entry, wait_node);

    entry->table->event = 1;
}

void poll_table_add(struct poll_table *table, struct wait_queue *queue)
{
    struct poll_table_entry *entry = kmalloc(sizeof(*entry), PAL_KERNEL);

    poll_table_entry_init(entry);
    entry->table = table;

    entry->wait_node.task = cpu_get_local()->current;
    entry->wait_node.wake_callback = poll_table_wait_queue_callback;

    wait_queue_register(queue, &entry->wait_node);

    list_add_tail(&table->entries, &entry->poll_table_node);
}

static void poll_table_unwait(struct poll_table *table)
{
    struct poll_table_entry *entry;

    list_foreach_take_entry(&table->entries, entry, poll_table_node) {
        wait_queue_unregister_wake(&entry->wait_node);
        kfree(entry);
    }
}

int sys_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    struct poll_table table = POLL_TABLE_INIT(table);
    struct file **filps = palloc_va(0, PAL_KERNEL);
    struct task *current = cpu_get_local()->current;
    unsigned int i;
    int ret;
    int exit_poll = 0;
    int event_count = 0;

    kp(KP_TRACE, "poll: %d\n", nfds);

    if (nfds == 0 && timeout < 0)
        return -EINVAL;

    for (i = 0; i < nfds; i++) {
        if (fds[i].fd <= -1) {
            filps[i] = NULL;
            continue;
        }

        ret = fd_get_checked(fds[i].fd, filps + i);
        if (ret)
            return ret;
    }

    if (timeout > 0)
        current->wake_up = scheduler_calculate_wakeup(timeout);
    else if (timeout == 0)
        exit_poll = 1;

    do {
        table.event = 0;

        for (i = 0; i < nfds; i++) {
            if (!filps[i])
                continue;

            if (filps[i]->ops->poll) {
                fds[i].revents = (filps[i]->ops->poll) (filps[i], &table, fds[i].events) & fds[i].events;

                if (fds[i].revents) {
                    event_count++;
                    exit_poll = 1;
                }

            } else {
                fds[i].revents = (POLLIN | POLLOUT) & fds[i].events;

                if (fds[i].revents) {
                    event_count++;
                    exit_poll = 1;
                }
            }
            kp(KP_TRACE, "poll %d: revents: %d...\n", i, fds[i].revents);
        }

        /* We do this check before the sleep so that if the scheduler woke us
         * up on a timeout, we do one last check for events. */
        if ((timeout > 0 && current->wake_up == 0)
            || (has_pending_signal(current)))
            exit_poll = 1;

        sleep_intr {
            /* Note - the scheduler sets 'wake_up' to zero if it woke us up because
             * the time occured. */
            if (!exit_poll && !table.event && (current->wake_up || timeout < 0))
                scheduler_task_yield();
        }

        poll_table_unwait(&table);
    } while (!exit_poll);

    current->wake_up = 0;

    if (event_count == 0 && has_pending_signal(current))
        event_count = -EINTR;

    pfree_va(filps, 0);
    return event_count;
}

