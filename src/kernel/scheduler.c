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
#include <mm/kmalloc.h>
#include <mm/memlayout.h>
#include <drivers/term.h>
#include <protura/dump_mem.h>
#include <mm/palloc.h>

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


/* ktasks is the current list of tasks the scheduler is holding.
 *
 * 'list' is the current list of struct task tasks that the scheduler could schedule.
 *
 * 'dead' is a list of tasks that have been killed and need to be cleaned up.
 *      - The scheduler handles this because a task can't clean itself up from
 *        it's own task. For example, a task can't free it's own stack.
 *
 * 'next_pid' is the next pid to assign to a new 'struct task'.
 *
 * 'lock' is a spinlock that needs to be held when you modify the list of tasks.
 */
static struct {
    struct spinlock lock;
    struct list_head list;

    struct list_head dead;

    pid_t next_pid;
} ktasks = {
    .lock = SPINLOCK_INIT("Task list lock"),
    .list = LIST_HEAD_INIT(ktasks.list),
    .dead = LIST_HEAD_INIT(ktasks.dead),
    .next_pid = 1,
};

pid_t scheduler_next_pid(void)
{
    return ktasks.next_pid++;
}

/* This functions is used as the starting point for all new forked threads.
 * The stack is manually setup by the initalization code, and this function is
 * the first function to be run.
 *
 * This function is necessary because we have to release the lock on ktasks
 * that we acquire in scheduler(). Normally this isn't a problem because a
 * task will call scheduler_task_yield() from it's context, and then get switch
 * to another context which exits the scheduler() and releases the lock on
 * ktasks.
 *
 * - But since this is our first entry for this task, we never called
 *   scheduler_task_yield() and thus need to free the lock on our own.
 */
void scheduler_task_entry(void)
{
    spinlock_release(&ktasks.lock);
}

void scheduler_task_add(struct task *task)
{
    /* Add this task to the beginning of the list of current tasks.
     * Adding to the beginning means it will get the next time-slice. */
    using_spinlock(&ktasks.lock)
        list_add(&ktasks.list, &task->task_list_node);
}

void scheduler_task_remove(struct task *task)
{
    /* Remove 'task' from the list of tasks to schedule. */
    using_spinlock(&ktasks.lock)
        list_del(&task->task_list_node);
}

static inline void __yield(struct task *current)
{
    arch_context_switch(&cpu_get_local()->scheduler, &current->context);
}

static inline void __sleep(struct task *t)
{
    t->state = TASK_SLEEPING;
    __yield(t);
}

void scheduler_task_yield(void)
{
    struct task *t = cpu_get_local()->current;
    using_spinlock(&ktasks.lock)
        __yield(t);
}

/* yield_preempt() sets the 'preempted' flag on the task before yielding.
 *
 * This is important beacuse if we preempt a task that's not RUNNABLE, it needs
 * to get scheduled anyway - If it doesn't want to be run anymore then it will
 * call __yield() directly when ready. */
void scheduler_task_yield_preempt(void)
{
    struct task *t = cpu_get_local()->current;

    t->preempted = 1;
    using_spinlock(&ktasks.lock)
        __yield(t);
}

void scheduler_task_waitms(uint32_t mseconds)
{
    struct task *t = cpu_get_local()->current;
    t->state = TASK_SLEEPING;
    t->wake_up = timer_get_ticks() + mseconds * (TIMER_TICKS_PER_SEC / 1000);
    kprintf("Wake-up tick: %d\n", t->wake_up);

    using_spinlock(&ktasks.lock)
        __yield(t);
}

void sys_sleep(uint32_t mseconds)
{
    scheduler_task_waitms(mseconds);
}

void scheduler_task_exit(int ret)
{
    struct task *t = cpu_get_local()->current;

    kprintf("Task %s exited: %d\n", t->name, ret);

    t->state = TASK_DEAD;

    using_spinlock(&ktasks.lock) {
        list_del(&t->task_list_node);
        list_add(&ktasks.dead, &t->task_list_node);

        __yield(t);
        panic("Exited task returned from __yield()!!!");
    }
}

void scheduler(void)
{
    struct task *t;

    /* We acquire but don't release this lock. This works because we
     * task_switch into other tasks, and those tasks will release the spinlock
     * for us, as well as acquire it for us before switching back into the
     * schedule */
    spinlock_acquire(&ktasks.lock);

    while (1) {

        /* First we handle any dead tasks and clean them up. */
        list_foreach_take_entry(&ktasks.dead, t, task_list_node) {
            kprintf("Task: %p\n", t);
            kprintf("freeing dead task %p\n", t->name);
            task_free(t);
        }


        uint32_t ticks = timer_get_ticks();

        /* Select the first RUNNABLE task in the schedule list.
         *
         * We do a simple foreach over every task in the list to check them.
         * After looping, we use list_ptr_is_head() to check if we reached the
         * end of the list or not - If we did, then we use the kidle task for
         * this cpu as our task.
         *
         * If we didn't reach the end of the list, then we found a task to run.
         * We use 'list_new_last' to rotate the list such that the node after
         * our selected task is the new head of the task list. This way, we
         * keep the same ordering and ensure that the next task we run is a
         * task we haven't checked yet. */
        list_foreach_entry(&ktasks.list, t, task_list_node) {

            /* If a task was preempted, then we start it again, reguardless of
             * it's current state */
            if (t->preempted) {
                t->preempted = 0;
                goto found_task;
            }

            switch (t->state) {
            case TASK_RUNNABLE:
                goto found_task;

            case TASK_SLEEPING:
                if (t->wake_up <= ticks && t->wake_up > 0) {
                    t->wake_up = 0;
                    goto found_task;
                }
                break;

            case TASK_RUNNING:
                break;

            /* Dead tasks can be removed and freed - They should already be
             * attached to ktasks.dead and not in this list. */
            case TASK_DEAD:
                break;

            /* TASK_NONE's should really not be in the scheduler list unless
             * there was an error */
            case TASK_NONE:
                break;
            }
        }

    found_task:

        if (list_ptr_is_head(&ktasks.list, &t->task_list_node)) {
            /* We execute this cpu's idle task if we didn't find a task to run */
            t = cpu_get_local()->kidle;
        } else {
            list_new_last(&ktasks.list, &t->task_list_node);
        }

        /* Set our current task equal to our new one, and set
         * it to RUNNING. After that we perform the actual switch
         *
         * Once the task_switch happens, */
        t->state = TASK_RUNNING;
        cpu_get_local()->current = t;

        task_switch(&cpu_get_local()->scheduler, t);

        /* Note - There's a big possibility that when we come back here
         * from a task switch, it's because the task was suspended or
         * similar. Thus, it's state may not be RUNNING, and if it's not we
         * don't want to change it to RUNNABLE */
        if (t->state == TASK_RUNNING)
            t->state = TASK_RUNNABLE;
        cpu_get_local()->current = NULL;
    }
}

void wakeup_list_init(struct wakeup_list *list)
{
    memset(list, 0, sizeof(*list));
}

void wakeup_list_add(struct wakeup_list *list, struct task *new)
{
    int i;

    for (i = 0; i < WAKEUP_LIST_MAX_TASKS; i++) {
        if (!list->tasks[i]) {
            list->tasks[i] = new;
            return ;
        }
    }
}

void wakeup_list_remove(struct wakeup_list *list, struct task *old)
{
    int i;

    for (i = 0; i < WAKEUP_LIST_MAX_TASKS; i++) {
        if (list->tasks[i] == old) {
            list->tasks[i] = NULL;
            return ;
        }
    }
}

void wakeup_list_wakeup(struct wakeup_list *list)
{
    int i;

    using_spinlock(&ktasks.lock)
        for (i = 0; i < WAKEUP_LIST_MAX_TASKS; i++)
            if (list->tasks[i])
                list->tasks[i]->state = TASK_RUNNABLE;
}

void wait_queue_init(struct wait_queue *queue)
{
    memset(queue, 0, sizeof(*queue));
    INIT_LIST_HEAD(&queue->queue);
    spinlock_init(&queue->lock, "wait queue lock");
}

void wait_queue_register(struct wait_queue *queue)
{
    struct task *t = cpu_get_local()->current;

    wait_queue_unregister();

    using_spinlock(&queue->lock) {
        list_add_tail(&queue->queue, &t->wait.node);
        t->wait.queue = queue;
    }
}

void wait_queue_unregister(void)
{
    struct task *t = cpu_get_local()->current;

    /* Check if t is currently registered for a queue before attempting to
     * remove it */
    if (!t->wait.queue)
        return ;

    /* We get the spinlock *before* checking if we're actually in the list,
     * because it's entire possible that we'll be removed from the list while
     * we're doing the check. */
    using_spinlock(&t->wait.queue->lock)
        if (list_node_is_in_list(&t->wait.node))
            list_del(&t->wait.node);
}

void wait_queue_wake(struct wait_queue *queue)
{
    struct task *t;

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
    using_spinlock(&queue->lock) {
        while (__list_first(&queue->queue)) {
            t = container_of(list_take_first(&queue->queue, struct wait_queue_node, node), struct task, wait);
            if (t->state == TASK_SLEEPING) {
                t->state = TASK_RUNNABLE;
                break;
            }
        }
    }
}

void wait_queue_wake_all(struct wait_queue *queue)
{
    struct wait_queue_node *node;
    struct task *t;

    using_spinlock(&queue->lock) {
        list_foreach_take_entry(&queue->queue, node, node) {
            t = container_of(node, struct task, wait);
            scheduler_task_wake(t);
        }
    }
}

