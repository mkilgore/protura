/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_TASK_H
#define INCLUDE_ARCH_TASK_H

#include <protura/types.h>
#include <protura/errors.h>
#include <protura/list.h>
#include <protura/stddef.h>
#include <protura/wait.h>
#include <arch/context.h>
#include <arch/paging.h>
#include <arch/cpu.h>

#define NOFILE 20

struct file;
struct inode;

enum task_state {
    TASK_NONE,
    TASK_SLEEPING,
    TASK_RUNNABLE,
    TASK_RUNNING,
    TASK_DEAD,
};

struct task {
    pid_t pid;
    enum task_state state;
    unsigned int preempted :1;
    unsigned int kernel :1;

    int wake_up; /* Tick number to wake-up on */

    struct list_node task_list_node;

    /* If this task is sleeping in a wait_queue, then this node is attached to that wait_queue */
    struct wait_queue_node wait;

    pd_t *page_dir;

    struct task *parent;
    context_t context;
    void *kstack_bot, *kstack_top;

    int killed; /* If non-zero, we've been killed and need to exit() */

    struct file *files[NOFILE];
    struct inode *cwd;

    char name[20];
};

/* Allocates a new task, assigning it a PID, intializing it's kernel
 * stack for it's first run, giving it a blank page-directory, and setting the
 * state to TASK_NONE.
 *
 * The caller should do any other initalization, set the state to
 * TASK_RUNNABLE, and then put the task into the scheduler list using
 * task_add. */
struct task *task_new(void);
struct task *task_fork(struct task *);
void task_init(struct task *);

/* Used for the 'fork()' syscall */
pid_t __fork(struct task *current);
pid_t sys_fork(void);
pid_t sys_getpid(void);
pid_t sys_getppid(void);

/* Used when a task is already killed and dead */
void task_free(struct task *);

void task_paging_init(struct task *);
void task_paging_free(struct task *);

void task_paging_copy_user(struct task *restrict new, struct task *restrict old);
struct task *task_fake_create(void);

struct task *task_kernel_new(char *name, int (*kernel_task) (void *), void *);
struct task *task_kernel_new_interruptable(char *name, int (*kernel_task) (void *), void *);

void __task_kernel(struct task *, char *name, int (*kernel_task) (void *), void *);
void __task_kernel_interuptable(struct task *, char *name, int (*kernel_task) (void *), void *);

void task_print(char *buf, size_t size, struct task *);
void task_switch(context_t *old, struct task *new);

int task_fd_get_empty(struct task *t);
void task_fd_release(struct task *t, int fd);
#define task_fd_assign(t, fd, filp) \
    ((t)->files[(fd)] = (filp))
#define task_fd_get(t, fd) \
    ((t)->files[(fd)])

static inline int task_fd_get_checked(struct task *t, int fd, struct file **filp)
{
    if (fd > NOFILE || fd < 0)
        return -EBADF;

    *filp = task_fd_get(t, fd);

    if (*filp == NULL)
        return -EBADF;

    return 0;
}

#define fd_get_empty() \
    task_fd_get_empty(cpu_get_local()->current)
#define fd_release(fd) \
    task_fd_release(cpu_get_local()->current, (fd));
#define fd_assign(fd, filp) \
    task_fd_assign(cpu_get_local()->current, (fd), (filp))
#define fd_get(fd) \
    task_fd_get(cpu_get_local()->current, (fd))
#define fd_get_checked(fd, filp) \
    task_fd_get_checked(cpu_get_local()->current, (fd), (filp))

extern const char *task_states[];

#endif
