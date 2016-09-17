
/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/list.h>
#include <arch/spinlock.h>
#include <protura/snprintf.h>
#include <protura/kassert.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/memlayout.h>
#include <protura/drivers/term.h>
#include <protura/dump_mem.h>
#include <protura/mm/palloc.h>
#include <protura/signal.h>
#include <protura/fs/procfs.h>
#include <protura/task_api.h>
#include <protura/fs/file.h>

#include <arch/fake_task.h>
#include <arch/kernel_task.h>
#include <arch/drivers/pic8259_timer.h>
#include <arch/idle_task.h>
#include <arch/context.h>
#include <arch/backtrace.h>
#include <arch/gdt.h>
#include <arch/idt.h>
#include <arch/paging.h>
#include <arch/asm.h>
#include <arch/cpu.h>
#include <arch/task.h>
#include <protura/scheduler.h>

void wait_queue_init(struct wait_queue *queue)
{
    memset(queue, 0, sizeof(*queue));
    list_head_init(&queue->queue);
    spinlock_init(&queue->lock, "wait queue lock");
}

void wait_queue_node_init(struct wait_queue_node *node)
{
    memset(node, 0, sizeof(*node));
    list_node_init(&node->node);
}

static inline void wait_queue_wake_node(struct wait_queue_node *node)
{
    if (node->wake_callback)
        (node->wake_callback) (node);

    scheduler_task_wake(node->task);
}

void wait_queue_register(struct wait_queue *queue, struct wait_queue_node *node)
{
    using_spinlock(&queue->lock) {
        list_add_tail(&queue->queue, &node->node);
        node->queue = queue;
    }
}

void wait_queue_unregister(struct wait_queue_node *node)
{
    /* Check if t is currently registered for a queue before attempting to
     * remove it */
    if (!node->queue)
        return ;

    /* We get the spinlock *before* checking if we're actually in the list,
     * because it's entire possible that we'll be removed from the list while
     * we're doing the check. */
    using_spinlock(&node->queue->lock)
        if (list_node_is_in_list(&node->node))
            list_del(&node->node);
}

static int __wait_queue_wake(struct wait_queue *queue)
{
    int waken = 0;
    struct wait_queue_node *node;

    /* dequeue takes the next sleeping task off of the wait queue and wakes it up.
     *
     * It's important that we wake-up the next SLEEPING task, and not just the
     * next task, beacuse to prevent lost wake-ups tasks will set themselves to
     * SLEEPING, register for the wait-queue, and then check if they actually
     * *need* to be in the wait-queue. If they don't, then they'll set their
     * state to RUNNING and then unregister themselves from the queue. If a
     * task changed it's state to RUNNING then it's as though it unregistered,
     * even though it hasn't actually unregistered yet.
     */
    list_foreach_take_entry(&queue->queue, node, node) {
        struct task *t = node->task;

        kp(KP_TRACE, "Wait queue %p: Task %p: %d\n", queue, t, t->state);
        if (t->state == TASK_SLEEPING || t->state == TASK_INTR_SLEEPING) {
            kp(KP_TRACE, "Wait queue %p: Waking task %s(%p)\n", queue, t->name, t);
            wait_queue_wake_node(node);
            waken++;
            break;
        }
    }

    return waken;
}

int wait_queue_unregister_wake(struct wait_queue_node *node)
{
    int ret = 0;

    using_spinlock(&node->queue->lock) {
        if (list_node_is_in_list(&node->node))
            list_del(&node->node);
        else
            ret = __wait_queue_wake(node->queue);
    }

    return ret;
}

int wait_queue_wake(struct wait_queue *queue)
{
    int waken = 0;

    using_spinlock(&queue->lock)
        waken = __wait_queue_wake(queue);

    return waken;
}

int wait_queue_wake_all(struct wait_queue *queue)
{
    int waken = 0;
    struct wait_queue_node *node;

    using_spinlock(&queue->lock) {
        list_foreach_take_entry(&queue->queue, node, node) {
            wait_queue_wake_node(node);
            waken++;
        }
    }

    return waken;
}

