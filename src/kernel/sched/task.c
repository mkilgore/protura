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
#include <protura/drivers/tty.h>
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
    work_init_task(&task->wait.on_complete, task);

    spinlock_init(&task->children_list_lock);

    task->addrspc = kmalloc(sizeof(*task->addrspc), PAL_KERNEL);
    address_space_init(task->addrspc);

    task->pid = scheduler_next_pid();

    task->state = TASK_RUNNING;

    task->kstack_bot = palloc_va(log2(KERNEL_STACK_PAGES), PAL_KERNEL);
    task->kstack_top = task->kstack_bot + PG_SIZE * KERNEL_STACK_PAGES - 1;

    credentials_init(&task->creds);

    arch_task_init(task);

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

void task_kernel_init(struct task *t, const char *name, int (*kernel_task)(void *), void *ptr)
{
    flag_set(&t->flags, TASK_FLAG_KERNEL);

    strcpy(t->name, name);

    /* We never exit to user-mode, so we have no frame */
    t->context.frame = NULL;

    arch_task_setup_stack_kernel(t, kernel_task, ptr);
}

struct task *task_kernel_new(const char *name, int (*kernel_task)(void *), void *ptr)
{
    struct task *t = task_new();

    if (!t)
        return NULL;

    task_kernel_init(t, name, kernel_task, ptr);
    return t;
}

struct task *task_user_new_exec(const char *exe)
{
    struct task *t;

    t = task_new();

    if (!t)
        return NULL;

    strncpy(t->name, exe, sizeof(t->name) - 1);
    t->name[sizeof(t->name) - 1] = '\0';

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

    new->tty = parent->tty;
    new->pgid = parent->pgid;
    new->session_id = parent->session_id;
    new->close_on_exec = parent->close_on_exec;
    new->sig_blocked = parent->sig_blocked;
    new->umask = parent->umask;

    using_creds(&parent->creds) {
        new->creds.uid = parent->creds.uid;
        new->creds.euid = parent->creds.euid;
        new->creds.suid = parent->creds.suid;

        new->creds.gid = parent->creds.gid;
        new->creds.egid = parent->creds.egid;
        new->creds.sgid = parent->creds.sgid;

        memcpy(&new->creds.sup_groups, &parent->creds.sup_groups, sizeof(new->creds.sup_groups));
    }

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

    /* If the session leader dies while controlling a tty, then we remove that
     * tty from every process in this same session. */
    if (flag_test(&t->flags, TASK_FLAG_SESSION_LEADER) && t->tty) {
        struct tty *tty = t->tty;
        scheduler_task_clear_sid_tty(tty, t->session_id);

        using_mutex(&tty->lock) {
            tty->session_id = 0;
            tty->fg_pgrp = 0;
        }
    }

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
