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
#include <protura/mm/palloc.h>
#include <protura/signal.h>

#include <arch/kernel_task.h>
#include <arch/drivers/pic8259_timer.h>
#include <arch/idle_task.h>
#include <arch/context.h>
#include <arch/backtrace.h>
#include <arch/asm.h>
#include <arch/cpu.h>
#include <arch/task.h>
#include <protura/scheduler.h>
#include "scheduler_internal.h"

struct sched_task_list ktasks = {
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
    /* Add this task to the *end* of the list.
     *
     * This prevents an interesting issue that can arise from a very-quickly
     * forking process preventing other processes from running. */
    using_spinlock(&ktasks.lock)
        list_add_tail(&ktasks.list, &task->task_list_node);
}

void scheduler_task_remove(struct task *task)
{
    /* Remove 'task' from the list of tasks to schedule. */
    using_spinlock(&ktasks.lock)
        list_del(&task->task_list_node);
}

/* Interrupt state is preserved across an arch_context_switch */
static inline void __yield(struct task *current)
{
    uint32_t eflags = ktasks.lock.eflags;

    arch_context_switch(&cpu_get_local()->scheduler, &current->context);

    ktasks.lock.eflags = eflags;
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

    flag_set(&t->flags, TASK_FLAG_PREEMPTED);
    using_spinlock(&ktasks.lock)
        __yield(t);
}

void scheduler_task_waitms(uint32_t mseconds)
{
    struct task *t = cpu_get_local()->current;
    t->state = TASK_INTR_SLEEPING;
    t->wake_up = timer_get_ticks() + mseconds * (TIMER_TICKS_PER_SEC / 1000);

    using_spinlock(&ktasks.lock)
        __yield(t);
}

int sys_sleep(int seconds)
{
    int ticks;
    struct task *t = cpu_get_local()->current;

    scheduler_task_waitms(seconds * 1000);

    if (t->wake_up == 0)
        return 0;

    ticks = (t->wake_up - timer_get_ticks()) / TIMER_TICKS_PER_SEC;

    if (ticks < 0)
        ticks = 0;

    return ticks;
}

void scheduler_task_mark_dead(struct task *t)
{
    t->state = TASK_DEAD;

    using_spinlock(&ktasks.lock) {
        list_del(&t->task_list_node);
        list_add(&ktasks.dead, &t->task_list_node);
    }
}

void scheduler_task_dead(void)
{
    scheduler_task_mark_dead(cpu_get_local()->current);

    scheduler_task_yield();
    panic("Dead task returned from yield()!!!");
}

int scheduler_task_exists(pid_t pid)
{
    struct task *t;
    int ret = -ESRCH;

    using_spinlock(&ktasks.lock) {
        list_foreach_entry(&ktasks.list, t, task_list_node) {
            if (t->pid == pid) {
                ret = 0;
                break;
            }
        }
    }

    return ret;
}

static void send_sig(struct task *t, int signal, int force)
{
    int notify_parent = 0;

    /* Deciding what to do when sending a signal is a little complex:
     *
     * If we get a SIGCONT, then:
     *   1. If we're actually stopped, then we set ret_signal to tell our
     *      parent about it.
     *   3. We force the task state to TASK_RUNNING if it is TASK_STOPPED
     *   2. We discard any currently pending 'stop' signals
     *
     * If we get a 'stop' (SIGSTOP, SIGTSTP, SIGTTOU, SIGTTIN), then:
     *   1. We discard any pending SIGCONT signals.
     *
     * If we get a SIGKILL (Unblockable), then:
     *   1. We force the task into TASK_RUNNING if it is TASK_STOPPED, thus
     *      forcing it to handle the SIGKILL and die.
     */
    if (signal == SIGCONT) {
        if (t->state == TASK_STOPPED) {
            t->ret_signal = TASK_SIGNAL_CONT;
            t->state = TASK_RUNNING;
            notify_parent = 1;
        }

        SIGSET_UNSET(&t->sig_pending, SIGSTOP);
        SIGSET_UNSET(&t->sig_pending, SIGTSTP);
        SIGSET_UNSET(&t->sig_pending, SIGTTOU);
        SIGSET_UNSET(&t->sig_pending, SIGTTIN);
    }

    if (signal == SIGSTOP || signal == SIGTSTP
            || signal == SIGTTOU || signal == SIGTTIN)
        SIGSET_UNSET(&t->sig_pending, SIGCONT);

    if (signal == SIGKILL && t->state == TASK_STOPPED)
        t->state = TASK_RUNNING;

    SIGSET_SET(&t->sig_pending, signal);
    if (force)
        SIGSET_UNSET(&t->sig_blocked, signal);

    scheduler_task_intr_wake(t);

    /* Notify our parent about any state changes due to signals.
     *
     * The NULL check is kinda a red-herring. Only kernel tasks have a NULL
     * parent. The rest of the tasks are guarenteed to always have a valid
     * parent pointer, even when being orphaned and adopped by init.
     * */
    if (notify_parent && t->parent)
        scheduler_task_wake(t->parent);
}

int scheduler_task_send_signal(pid_t pid, int signal, int force)
{
    int ret = 0;
    struct task *t;

    if (signal < 1 || signal > NSIG)
        return -EINVAL;

    kp(KP_TRACE, "signal: %d to %d\n", signal, pid);

    ret = -ESRCH;

    using_spinlock(&ktasks.lock) {
        list_foreach_entry(&ktasks.list, t, task_list_node) {
            kp(KP_TRACE, "signal: Checking Pid %d\n", t->pid);
            if (pid > 0 && t->pid == pid) {
                send_sig(t, signal, force);
                ret = 0;
                break;
            } else if (pid < 0 && t->pgid == -pid) {
                kp(KP_TRACE, "signal: Sending signal %d to %d\n", signal, t->pid);
                send_sig(t, signal, force);
                ret = 0;
            }
        }
    }

    return ret;
}

void scheduler_task_clear_sid_tty(struct tty *tty, pid_t sid)
{
    struct task *t;
    using_spinlock(&ktasks.lock) {
        list_foreach_entry(&ktasks.list, t, task_list_node) {
            if (t->session_id == sid)
                atomic_ptr_cmpxchg(&t->tty, tty, NULL);
        }
    }
}

struct task *scheduler_task_get(pid_t pid)
{
    struct task *t, *found = NULL;

    spinlock_acquire(&ktasks.lock);

    list_foreach_entry(&ktasks.list, t, task_list_node) {
        if (t->pid == pid) {
            found = t;
            break;
        }
    }

    if (found)
        return found;

    spinlock_release(&ktasks.lock);
    return 0;
}

void scheduler_task_put(struct task *t)
{
    spinlock_release(&ktasks.lock);
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
            kp(KP_TRACE, "Task: %p\n", t);
            kp(KP_TRACE, "freeing dead task %p\n", t->name);
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
             * it's current state. It's possible they aren't actually
             * TASK_RUNNING, which is why this check is needed. */
            if (flag_test(&t->flags, TASK_FLAG_PREEMPTED)) {
                flag_clear(&t->flags, TASK_FLAG_PREEMPTED);
                goto found_task;
            }

            switch (t->state) {
            case TASK_RUNNING:
                if (!flag_test(&t->flags, TASK_FLAG_RUNNING))
                    goto found_task;
                break;

            case TASK_INTR_SLEEPING:
            case TASK_SLEEPING:
                if (t->wake_up <= ticks && t->wake_up > 0) {
                    t->wake_up = 0;
                    goto found_task;
                }
                break;

            /* Stopped tasks are skipped */
            case TASK_STOPPED:
                break;

            /* Dead tasks can be removed and freed - They should already be
             * attached to ktasks.dead and not in this list.
             *
             * Zombie's on the other hand, are waiting to be reaped by their
             * parent. */
            case TASK_DEAD:
            case TASK_ZOMBIE:
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

        /* Set the running flag as we prepare to enter this task */
        flag_set(&t->flags, TASK_FLAG_RUNNING);
        cpu_get_local()->current = t;

        task_switch(&cpu_get_local()->scheduler, t);

        cpu_get_local()->current = NULL;
        flag_clear(&t->flags, TASK_FLAG_RUNNING);
    }
}
