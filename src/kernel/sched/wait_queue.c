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
    work_schedule(&node->on_complete);
}

void wait_queue_register(struct wait_queue *queue, struct wait_queue_node *node)
{
    using_spinlock(&queue->lock) {
        if (!list_node_is_in_list(&node->node)) {
            list_add_tail(&queue->queue, &node->node);

            node->queue = queue;
        } else if (node->queue != queue) {
            panic("Node %p: Attempting to join multiple wait-queues\n", node);
        }
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
    using_spinlock(&node->queue->lock) {
        if (list_node_is_in_list(&node->node))
            list_del(&node->node);
        else
            kp(KP_TRACE, "Node %p: Queue is set but not is not in list\n", node);
    }
}

int wait_queue_wake(struct wait_queue *queue)
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
