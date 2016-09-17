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
 *
 * Note that the locking is a little tricky to understand. In some cases, a
 * standard using_spinlock, with the normal spinlock_acquire and
 * spinlock_release is perfectly fine. However, when the lock is going to
 * suround a context switch, it means that the lock itself is going to be
 * locked and unlocked in two different contexts. Thus, the eflags value that
 * is normally saved in the spinlock would be preserved across the context,
 * corrupting the interrupt state.
 *
 * The solution is to save the contents of the eflags register into the current
 * task's context, and then restore it on a context switch from the saved
 * value in the new task's context. Since we're saving the eflags ourselves,
 * it's necessary to call the 'noirq' versions of spinlock, which do nothing to
 * change the interrupt state.
 */
static struct {
    struct spinlock lock;
    list_head_t list;

    list_head_t dead;

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
    SIGSET_SET(&t->sig_pending, signal);
    if (force)
        SIGSET_UNSET(&t->sig_blocked, signal);

    scheduler_task_intr_wake(t);
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

static int scheduler_tasks_read(void *p, size_t size, size_t *len)
{
    struct task *t;

    *len = snprintf(p, size, "Pid\tPPid\tPGid\tState\tKilled\tName\n");

    using_spinlock(&ktasks.lock) {
        list_foreach_entry(&ktasks.list, t, task_list_node) {
            int state = t->state;

            *len += snprintf(p + *len, size - *len,
                    "%d\t%d\t%d\t%s\t%d\t\"%s\"\n",
                    t->pid,
                    (t->parent)? t->parent->pid: 0,
                    t->pgid,
                    task_states[state],
                    flag_test(&t->flags, TASK_FLAG_KILLED),
                    t->name);

            if (*len == size)
                break;
        }
    }

    return 0;
}

struct procfs_entry_ops tasks_ops = {
    .readpage = scheduler_tasks_read,
};

static void fill_task_api_info(struct task_api_info *tinfo, struct task *task)
{
    memset(tinfo, 0, sizeof(*tinfo));

    tinfo->is_kernel = flag_test(&task->flags, TASK_FLAG_KERNEL);
    tinfo->pid = task->pid;

    if (task->parent)
        tinfo->ppid = task->parent->pid;
    else
        tinfo->ppid = 0;

    tinfo->pgid = task->pgid;

    switch (task->state) {
    case TASK_RUNNING:
        tinfo->state = TASK_API_RUNNING;
        break;

    case TASK_ZOMBIE:
        tinfo->state = TASK_API_ZOMBIE;
        break;

    case TASK_SLEEPING:
        tinfo->state = TASK_API_SLEEPING;
        break;

    case TASK_INTR_SLEEPING:
        tinfo->state = TASK_API_INTR_SLEEPING;
        break;

    case TASK_DEAD:
    case TASK_NONE:
        tinfo->state = TASK_API_NONE;
        break;
    }

    memcpy(&tinfo->close_on_exec, &task->close_on_exec, sizeof(tinfo->close_on_exec));
    memcpy(&tinfo->sig_pending, &task->sig_pending, sizeof(tinfo->sig_pending));
    memcpy(&tinfo->sig_blocked, &task->sig_blocked, sizeof(tinfo->sig_blocked));

    memcpy(tinfo->name, task->name, sizeof(tinfo->name));
}

static int scheduler_task_api_read(struct file *filp, void *p, size_t size)
{
    struct task_api_info tinfo;
    struct task *task, *found = NULL;
    pid_t last_pid = filp->offset;
    pid_t found_pid = -1;

    using_spinlock(&ktasks.lock) {
        list_foreach_entry(&ktasks.list, task, task_list_node) {
            if (task->state == TASK_DEAD
                || task->state == TASK_NONE)
                continue;

            if (task->pid > last_pid
                && (found_pid == -1 || task->pid < found_pid)) {
                found = task;
                found_pid = task->pid;
            }
        }

        if (!found)
            break;

        fill_task_api_info(&tinfo, found);
    }

    if (found) {
        filp->offset = found_pid;
        if (size > sizeof(tinfo))
            memcpy(p, &tinfo, sizeof(tinfo));
        else
            memcpy(p, &tinfo, size);

        return size;
    } else {
        return 0;
    }
}

static int task_api_fill_mem_info(struct task_api_mem_info *info)
{
    struct vm_map *map;
    struct task *task;
    int region;

    task = scheduler_task_get(info->pid);
    if (!task)
        return -ESRCH;

    list_foreach_entry(&task->addrspc->vm_maps, map, address_space_entry) {
        region = info->region_count++;

        if (region > ARRAY_SIZE(info->regions))
            break;

        info->regions[region].start = (uintptr_t)map->addr.start;
        info->regions[region].end = (uintptr_t)map->addr.end;

        info->regions[region].is_read= flag_test(&map->flags, VM_MAP_READ);
        info->regions[region].is_write = flag_test(&map->flags, VM_MAP_WRITE);
        info->regions[region].is_exec = flag_test(&map->flags, VM_MAP_EXE);
    }

    scheduler_task_put(task);

    return 0;
}

static int task_api_fill_file_info(struct task_api_file_info *info)
{
    int i;
    struct task *task;

    task = scheduler_task_get(info->pid);
    if (!task)
        return -ESRCH;

    for (i = 0; i < NOFILE; i++) {
        struct file *filp = task_fd_get(task, i);

        if (!filp) {
            info->files[i].in_use = 0;
            continue;
        }

        info->files[i].in_use = 1;

        if (inode_is_pipe(filp->inode))
            info->files[i].is_pipe = 1;

        if (flag_test(&filp->flags, FILE_WRITABLE))
            info->files[i].is_writable = 1;

        if (flag_test(&filp->flags, FILE_READABLE))
            info->files[i].is_readable = 1;

        if (flag_test(&filp->flags, FILE_NONBLOCK))
            info->files[i].is_nonblock= 1;

        if (flag_test(&filp->flags, FILE_APPEND))
            info->files[i].is_append = 1;

        info->files[i].inode = filp->inode->ino;
        info->files[i].dev = DEV_TO_USERSPACE(filp->inode->sb_dev);
        info->files[i].mode = filp->inode->mode;
        info->files[i].offset = filp->offset;
        info->files[i].size = filp->inode->size;
    }

    scheduler_task_put(task);

    return 0;
}

static int scheduler_task_api_ioctl(struct file *filp, int cmd, uintptr_t ptr)
{
    struct task_api_mem_info *mem_info;
    struct task_api_file_info *file_info;
    int ret;

    switch (cmd) {
    case TASKIO_MEM_INFO:
        mem_info = (struct task_api_mem_info *)ptr;
        ret = user_check_region(mem_info, sizeof(*mem_info), F(VM_MAP_READ) | F(VM_MAP_WRITE));
        if (ret)
            return ret;

        return task_api_fill_mem_info(mem_info);

    case TASKIO_FILE_INFO:
        file_info = (struct task_api_file_info *)ptr;
        ret = user_check_region(file_info, sizeof(*file_info), F(VM_MAP_READ) | F(VM_MAP_WRITE));
        if (ret)
            return ret;

        return task_api_fill_file_info(file_info);
    }

    return -EINVAL;
}

struct procfs_entry_ops task_api_ops = {
    .read = scheduler_task_api_read,
    .ioctl = scheduler_task_api_ioctl,
};

