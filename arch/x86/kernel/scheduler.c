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
#include <protura/spinlock.h>
#include <protura/snprintf.h>
#include <mm/kmalloc.h>
#include <mm/memlayout.h>
#include <arch/pmalloc.h>
#include <drivers/term.h>
#include <protura/dump_mem.h>

#include <arch/fake_task.h>
#include <arch/kernel_task.h>
#include <arch/drivers/pic8259_timer.h>
#include <arch/idle_task.h>
#include <arch/context.h>
#include <arch/backtrace.h>
#include <arch/gdt.h>
#include <arch/idt.h>
#include "irq_handler.h"
#include <arch/paging.h>
#include <arch/asm.h>
#include <arch/cpu.h>
#include <arch/task.h>


static struct {
    struct spinlock lock;
    struct list_head list;

    struct list_head empty;

    pid_t next_pid;
} ktasks = {
    .lock = SPINLOCK_INIT("Task list lock"),
    .list = LIST_HEAD_INIT(ktasks.list),
    .empty = LIST_HEAD_INIT(ktasks.empty),
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
 * that we acquire from scheduler(). Normally this isn't a problem because a
 * task will call into scheduler() from it's context, and then get switch to
 * another context which exits the scheduler() and releases the lock on ktasks
 * - But since this is our first entry for this task, we never called
 *   scheduler() and thus need to clean-up on our own.
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

void scheduler_task_yield(void)
{
    using_spinlock(&ktasks.lock)
        arch_context_switch(&cpu_get_local()->scheduler, &cpu_get_local()->current->context);
}

void scheduler_task_sleep(uint32_t mseconds)
{
    struct task *t = cpu_get_local()->current;
    t->state = TASK_SLEEPING;
    t->wake_up = timer_get_ticks() + mseconds * (TIMER_TICKS_PER_SEC / 1000);
    kprintf("Wake-up tick: %d\n", t->wake_up);
    scheduler_task_yield();
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
            switch (t->state) {
            case TASK_RUNNABLE:
                goto found_task;

            case TASK_SLEEPING:
                if (t->wake_up <= ticks && t->wake_up > 0)
                    goto found_task;
                break;

            case TASK_RUNNING:
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
         * don't want to change it to RUNNING */
        if (t->state == TASK_RUNNING)
            t->state = TASK_RUNNABLE;
        cpu_get_local()->current = NULL;
    }
}

