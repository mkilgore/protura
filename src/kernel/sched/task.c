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
#include <protura/snprintf.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/memlayout.h>
#include <protura/drivers/term.h>
#include <protura/dump_mem.h>
#include <protura/scheduler.h>
#include <protura/task.h>
#include <protura/mm/palloc.h>
#include <protura/fs/inode.h>
#include <protura/fs/file.h>
#include <protura/fs/fs.h>
#include <protura/wait.h>
#include <protura/signal.h>
#include <protura/task_api.h>

#include <arch/spinlock.h>
#include <arch/fake_task.h>
#include <arch/kernel_task.h>
#include <arch/idle_task.h>
#include <arch/context.h>
#include <arch/backtrace.h>
#include <arch/gdt.h>
#include <arch/idt.h>
#include <arch/paging.h>
#include <arch/asm.h>
#include <arch/cpu.h>
#include <arch/task.h>

#define KERNEL_STACK_PAGES 2

static atomic_t total_tasks = ATOMIC_INIT(0);

const char *task_states[] = {
    [TASK_NONE]          = "no state",
    [TASK_SLEEPING]      = "sleep",
    [TASK_INTR_SLEEPING] = "isleep",
    [TASK_RUNNING]       = "running",
    [TASK_ZOMBIE]        = "zombie",
    [TASK_DEAD]          = "dead",
};

void task_print(char *buf, size_t size, struct task *task)
{
    snprintf(buf, size, "Task: %s\n"
                        "Pid: %d\n"
                        "Parent: %d\n"
                        "State: %s\n"
                        "Killed: %d\n"
                        , task->name
                        , task->pid
                        , task->parent->pid
                        , task_states[task->state]
                        , flag_test(&task->flags, TASK_FLAG_KILLED)
                        );
}

void task_init(struct task *task)
{
    memset(task, 0, sizeof(*task));

    list_node_init(&task->task_list_node);
    list_node_init(&task->task_sibling_list);
    list_head_init(&task->task_children);
    wait_queue_node_init(&task->wait);
    spinlock_init(&task->children_list_lock, "Task child list");

    task->addrspc = kmalloc(sizeof(*task->addrspc), PAL_KERNEL);
    address_space_init(task->addrspc);

    task->pid = scheduler_next_pid();

    task->state = TASK_RUNNING;

    task->kstack_bot = palloc_va(log2(KERNEL_STACK_PAGES), PAL_KERNEL);
    task->kstack_top = task->kstack_bot + PG_SIZE * KERNEL_STACK_PAGES - 1;

    kp(KP_TRACE, "Created task %d\n", task->pid);
}

/* Initializes a new allocated task */
struct task *task_new(void)
{
    struct task *task;

    if (atomic_get(&total_tasks) >= CONFIG_TASK_MAX) {
        kp(KP_WARNING, "!!!!MAX TASKS REACHED, task_new() REFUSED!!!!\n");
        return NULL;
    }

    atomic_inc(&total_tasks);

    task = kmalloc(sizeof(*task), PAL_KERNEL);
    kp(KP_TRACE, "task kmalloc: %p\n", task);
    if (!task)
        return NULL;

    task_init(task);

    return task;
}

static struct task *task_kernel_generic(struct task *t, const char *name, int (*kernel_task)(void *), void *ptr, int is_interruptable)
{
    flag_set(&t->flags, TASK_FLAG_KERNEL);

    strcpy(t->name, name);

    /* We never exit to user-mode, so we have no frame */
    t->context.frame = NULL;

    if (is_interruptable)
        arch_task_setup_stack_kernel_interruptable(t, kernel_task, ptr);
    else
        arch_task_setup_stack_kernel(t, kernel_task, ptr);

    return t;
}

/* The main difference here is that the interruptable kernel task entry
 * function will enable interrupts before calling 'kernel_task', regular kernel
 * task's won't. */
struct task *task_kernel_new_interruptable(const char *name, int (*kernel_task)(void *), void *ptr)
{
    struct task *t = task_new();

    if (!t)
        return NULL;

    return task_kernel_generic(t, name, kernel_task, ptr, 1);
}

struct task *task_kernel_new(const char *name, int (*kernel_task)(void *), void *ptr)
{
    struct task *t = task_new();

    if (!t)
        return NULL;

    return task_kernel_generic(t, name, kernel_task, ptr, 0);
}

struct task *task_user_new_exec(const char *exe)
{
    struct task *t;

    t = task_new();

    if (!t)
        return NULL;

    strncpy(t->name, exe, sizeof(t->name) - 1);
    t->name[sizeof(t->name) - 1] = '\0';

    flag_set(&t->flags, TASK_FLAG_USER_PTR_CHECK);

    arch_task_setup_stack_user_with_exec(t, exe);

    t->cwd = inode_dup(ino_root);

    return t;
}

struct task *task_user_new(void)
{
    struct task *t;

    t = task_new();

    if (!t)
        return NULL;

    flag_set(&t->flags, TASK_FLAG_USER_PTR_CHECK);

    arch_task_setup_stack_user(t);

    return t;
}

/* Creates a new processes that is a copy of 'parent'.
 * Note, the userspace code/data/etc. is copied, but not the kernel-space stuff
 * like the kernel stack. */
struct task *task_fork(struct task *parent)
{
    int i;
    struct task *new = task_new();
    if (!new)
        return NULL;

    strcpy(new->name, parent->name);

    arch_task_setup_stack_user(new);
    address_space_copy(new->addrspc, parent->addrspc);

    new->cwd = inode_dup(parent->cwd);

    for (i = 0; i < NOFILE; i++)
        if (parent->files[i])
            new->files[i] = file_dup(parent->files[i]);

    new->pgid = parent->pgid;
    new->session_id = parent->session_id;
    new->close_on_exec = parent->close_on_exec;
    new->sig_blocked = parent->sig_blocked;

    new->parent = parent;
    memcpy(new->context.frame, parent->context.frame, sizeof(*new->context.frame));

    return new;
}

void task_free(struct task *t)
{
    atomic_dec(&total_tasks);

    /* If this task wasn't yet killed, then we do it now */
    if (!flag_test(&t->flags, TASK_FLAG_KILLED))
        task_make_zombie(t);

    pfree_va(t->kstack_bot, log2(KERNEL_STACK_PAGES));

    kfree(t);
}

void task_make_zombie(struct task *t)
{
    struct task *child;
    int i;

    kp(KP_TRACE, "zombie: %d\n", t->pid);

    flag_set(&t->flags, TASK_FLAG_KILLED);

    /* Children of Zombie's are inherited by PID1. */
    using_spinlock(&task_pid1->children_list_lock) {
        list_foreach_take_entry(&t->task_children, child, task_sibling_list) {
            /* The atomic swap guarentees consistency of the child->parent
             * pointer.
             *
             * There is no actual race here, even without a lock on the
             * 'child->parent' field, because a task will always be set to
             * TASK_ZOMBIE *before* attempting to wake it's parent.
             *
             * The race would be if 'child' is in sys_exit() while we're making
             * it's current parent a zombie. It's not an issue because
             * sys_exit() always sets 'child->state' to TASK_ZOMBIE *before*
             * caling scheduler_task_wake(child->parent). 
             *
             * Thus, by swaping in task_pid1 as the parent before checking for
             * TASK_ZOMBIE, we guarentee that we get the correct functionality:
             * If 'child->state' isn't TASK_ZOMBIE: 
             *   There's no issue because even if they're in sys_exit(), they
             *   haven't attempted to wake up the parent yet.
             *
             * If 'child->state' is TASK_ZOMBIE:
             *   Then we call scheduler_task_wake(task_pid1) to ensure PID1
             *   gets the wake-up. The worst case here is that PID1 recieves
             *   two wake-ups - No big deal.
             */
            atomic_ptr_swap(&child->parent, task_pid1);

            kp(KP_TRACE, "Init: Inheriting child %d\n", child->pid);
            list_move(&task_pid1->task_children, &child->task_sibling_list);

            if (child->state == TASK_ZOMBIE)
                scheduler_task_send_signal(1, SIGCHLD, 0);
        }
    }

    for (i = 0; i < NOFILE; i++) {
        if (t->files[i]) {
            kp(KP_TRACE, "closing file %d\n", i);
            vfs_close(t->files[i]);
        }
    }

    if (t->cwd)
        inode_put(t->cwd);

    if (!flag_test(&t->flags, TASK_FLAG_KERNEL)) {
        address_space_clear(t->addrspc);
        kfree(t->addrspc);
    }

    t->state = TASK_ZOMBIE;

    if (t->parent)
        scheduler_task_send_signal(t->parent->pid, SIGCHLD, 0);

    kp(KP_TRACE, "Task %s(%p): zombie\n", t->name, t);
}

/* Note that it's important that inbetween 'task_fd_get_empty()' returning an
 * fd, and the fd being assigned a pointer value, that we don't accidentally
 * return the same fd a second time if it is requested. IE. Two threads both
 * requesting fd's should recieve unique fd's, even if they're calling
 * task_fd_get_empty() at the exact same time.
 *
 * Thus, empty entries in the file table are marked with NULL, and we do an
 * atomic compare-and-swap to swap the NULL with a non-null temporary value
 * (-1). This non-null value means that subsiquent calls to task_fd_get_empty()
 * won't return the same fd even if the returned fd's have yet to be assigned.
 * (The caller will replace the temporary value with a valid filp after the
 * call).
 */
int task_fd_get_empty(struct task *t)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(t->files); i++)
        if (cmpxchg(t->files + i, 0, -1) == 0)
            return i;

    return -1;
}

void task_fd_release(struct task *t, int fd)
{
    t->files[fd] = NULL;
}

pid_t __fork(struct task *current, pid_t pgrp)
{
    struct task *new;
    new = task_fork(current);

    if (new)
        kp(KP_TRACE, "New task: %d\n", new->pid);
    else
        kp(KP_TRACE, "Fork failed\n");

    if (new) {
        if (pgrp == 0)
            new->pgid = new->pid;
        else if (pgrp > 0)
            new->pgid = pgrp;

        kp(KP_TRACE, "Task %s: Locking list of children\n", current->name);
        using_spinlock(&current->children_list_lock)
            list_add(&current->task_children, &new->task_sibling_list);
        kp(KP_TRACE, "Task %s: Unlocking list of children: %d\n", current->name, list_empty(&current->task_children));

        irq_frame_set_syscall_ret(new->context.frame, 0);
        scheduler_task_add(new);
    }

    if (new)
        return new->pid;
    else
        return -1;
}

pid_t sys_fork(void)
{
    struct task *t = cpu_get_local()->current;
    return __fork(t, -1);
}

pid_t sys_fork_pgrp(pid_t pgrp)
{
    struct task *t = cpu_get_local()->current;
    return __fork(t, pgrp);
}

pid_t sys_getpid(void)
{
    struct task *t = cpu_get_local()->current;
    return t->pid;
}

pid_t sys_getppid(void)
{
    struct task *t = cpu_get_local()->current;
    if (t->parent)
        return t->parent->pid;
    else
        return -1;
}

void sys_exit(int code)
{
    struct task *t = cpu_get_local()->current;
    t->ret_code = code;

    kp(KP_TRACE, "Exit! %s(%p):%d\n", t->name, t, code);

    /* At this point, we have to disable interrupts to ensure we don't get
     * rescheduled. We're deleting the majority of our task information, and a
     * reschedule back to us may not work after that. */
    irq_disable();

    /* We're going to delete our address-space, including our page-table, so we
     * need to switch to the kernel's to ensure we don't attempt to keep using
     * the deleted page-table. */
    arch_address_space_switch_to_kernel();

    task_make_zombie(t);

    /* Compiler barrier guarentee's that t->parent will not be read *before*
     * t->state is set to TASK_ZOMBIE. See details in task_make_zombie(). */
    barrier();

    if (t->parent)
        scheduler_task_wake(t->parent);

    /* If we're a kernel process, we will never be wait()'d on, so we mark
     * ourselves dead */
    if (flag_test(&t->flags, TASK_FLAG_KERNEL))
        scheduler_task_mark_dead(t);

    scheduler_task_yield();
    panic("scheduler_task_yield() returned after sys_exit()!!!\n");
}

pid_t sys_wait(int *ret)
{
    return sys_waitpid(-1, ret, 0);
}

pid_t sys_waitpid(pid_t childpid, int *wstatus, int options)
{
    int have_child = 0, have_no_children = 0;
    struct task *child = NULL;
    struct task *t = cpu_get_local()->current;

    /* We enter a 'sleep loop' here. We sleep until we're woke-up directly,
     * which is fine because we're waiting for any children to call sys_exit(),
     * which will wake us up. */
  sleep_again:
    sleep_intr {
        kp(KP_TRACE, "Task %s: Locking child list for wait4\n", t->name);
        using_spinlock(&t->children_list_lock) {
            if (list_empty(&t->task_children)) {
                have_no_children = 1;
                break;
            }

            list_foreach_entry(&t->task_children, child, task_sibling_list) {
                kp(KP_TRACE, "Checking child %s(%p, %d)\n", child->name, child, child->pid);
                if (childpid == 0) {
                    if (child->pgid != t->pgid)
                        continue;
                } else if (childpid > 0) {
                    if (child->pid != childpid)
                        continue;
                } else if (childpid < -1) {
                    if (child->pgid != -childpid)
                        continue;
                }
                if (child->state == TASK_ZOMBIE) {
                    list_del(&child->task_sibling_list);
                    kp(KP_TRACE, "Found zombie child: %d\n", child->pid);
                    have_child = 1;
                    break;
                }
            }
        }
        kp(KP_TRACE, "Task %s: Unlocking child list for wait4\n", t->name);

        if (!have_no_children && !have_child && !(options & WNOHANG)) {
            scheduler_task_yield();
            if (has_pending_signal(t))
                return -ERESTARTSYS;
            goto sleep_again;
        }
    }

    if (!have_child && options & WNOHANG)
        return 0;

    if (!have_child)
        return -ECHILD;

    if (wstatus)
        *wstatus = child->ret_code;

    scheduler_task_mark_dead(child);
    return child->pid;
}

int sys_dup(int oldfd)
{
    struct task *current = cpu_get_local()->current;
    struct file *filp = fd_get(oldfd);
    int newfd;
    int ret;

    ret = fd_get_checked(oldfd, &filp);
    if (ret)
        return ret;

    newfd = fd_get_empty();

    fd_assign(newfd, file_dup(filp));

    FD_CLR(newfd, &current->close_on_exec);

    return newfd;
}

int sys_dup2(int oldfd, int newfd)
{
    struct task *current = cpu_get_local()->current;
    struct file *old_filp;
    struct file *new_filp;
    int ret;

    ret = fd_get_checked(oldfd, &old_filp);
    if (ret)
        return ret;

    if (newfd > NOFILE || newfd < 0)
        return -EBADF;

    new_filp = fd_get(newfd);

    if (new_filp)
        vfs_close(new_filp);

    fd_assign(newfd, file_dup(old_filp));

    FD_CLR(newfd, &current->close_on_exec);

    return newfd;
}

int sys_setpgid(pid_t pid, pid_t pgid)
{
    struct task *current = cpu_get_local()->current;

    if (pid) {
        struct task *t = scheduler_task_get(pid);

        if (!t)
            return -ESRCH;

        if (!pgid)
            pgid = pid;

        t->pgid = pgid;

        scheduler_task_put(t);
        return 0;
    }

    if (!pgid)
        pgid = current->pid;

    current->pgid = pgid;

    return 0;
}

int sys_getpgrp(pid_t *pgrp)
{
    struct task *current = cpu_get_local()->current;

    *pgrp = current->pgid;

    return 0;
}


