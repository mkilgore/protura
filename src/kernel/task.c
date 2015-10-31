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
#include <mm/kmalloc.h>
#include <mm/memlayout.h>
#include <drivers/term.h>
#include <protura/dump_mem.h>
#include <protura/scheduler.h>
#include <protura/task.h>
#include <mm/palloc.h>
#include <fs/inode.h>
#include <fs/file.h>
#include <fs/fs.h>

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

const char *task_states[] = {
    [TASK_NONE]     = "no state",
    [TASK_SLEEPING] = "sleeping",
    [TASK_RUNNABLE] = "runnable",
    [TASK_RUNNING]  = "running",
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
                        , task->killed
                        );
}

static struct task *task_kernel_generic(struct task *t, char *name, int (*kernel_task)(void *), void *ptr, int is_interruptable)
{
    t->kernel = 1;

    strcpy(t->name, name);

    /* We never exit to user-mode, so we have no frame */
    t->context.frame = NULL;

    if (is_interruptable)
        arch_task_setup_stack_kernel_interruptable(t, kernel_task, ptr);
    else
        arch_task_setup_stack_kernel(t, kernel_task, ptr);

    t->state = TASK_RUNNABLE;
    return t;
}

/* The main difference here is that the interruptable kernel task entry
 * function will enable interrupts before calling 'kernel_task', regular kernel
 * task's won't. */
struct task *task_kernel_new_interruptable(char *name, int (*kernel_task)(void *), void *ptr)
{
    struct task *t = task_new();

    if (!t)
        return NULL;

    return task_kernel_generic(t, name, kernel_task, ptr, 1);
}

struct task *task_kernel_new(char *name, int (*kernel_task)(void *), void *ptr)
{
    struct task *t = task_new();

    if (!t)
        return NULL;

    return task_kernel_generic(t, name, kernel_task, ptr, 0);
}

void task_init(struct task *task)
{
    memset(task, 0, sizeof(*task));

    list_node_init(&task->task_list_node);
    wait_queue_node_init(&task->wait);

    task->addrspc = kmalloc(sizeof(*task->addrspc), PAL_KERNEL);
    address_space_init(task->addrspc);

    task->pid = scheduler_next_pid();

    task->state = TASK_RUNNABLE;

    task->kstack_bot = palloc_multiple(PAL_KERNEL, log2(KMEM_STACK_LIMIT));
    task->kstack_top = task->kstack_bot + PG_SIZE * KMEM_STACK_LIMIT - 1;

    kprintf("Created task %d\n", task->pid);
}

/* Initializes a new allocated task */
struct task *task_new(void)
{
    struct task *task;

    task = kmalloc(sizeof(*task), PAL_KERNEL);
    if (!task)
        return NULL;

    task_init(task);

    return task;
}

struct task *task_user_new(const char *exe)
{
    struct task *t;

    t = task_new();

    arch_task_setup_stack_user_with_exec(t, exe);

    t->cwd = inode_dup(ino_root);

    t->state = TASK_RUNNABLE;

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

    snprintf(new->name, sizeof(new->name), "Chld: %s", parent->name);

    arch_task_setup_stack_user(new);
    address_space_copy(parent->addrspc, new->addrspc);

    new->cwd = inode_dup(parent->cwd);

    for (i = 0; i < NOFILE; i++)
        new->files[i] = file_dup(parent->files[i]);

    new->parent = parent;
    memcpy(new->context.frame, parent->context.frame, sizeof(*new->context.frame));

    return new;
}

void task_free(struct task *t)
{
    address_space_clear(t->addrspc);
    kfree(t->addrspc);

    pfree_multiple(t->kstack_bot, log2(KERNEL_STACK_PAGES));

    kfree(t);
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

pid_t __fork(struct task *current)
{
    struct task *new;
    new = task_fork(current);

    kprintf("New task: %d\n", new->pid);

    if (new) {
        scheduler_task_add(new);

        new->context.frame->eax = 0;
        current->context.frame->eax = new->pid;
    } else {
        current->context.frame->eax = -1;
    }

    return new->pid;
}

pid_t sys_fork(void)
{
    struct task *t = cpu_get_local()->current;
    return __fork(t);
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



