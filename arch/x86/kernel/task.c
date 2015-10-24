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
#include <mm/palloc.h>
#include <fs/inode.h>
#include <fs/file.h>
#include <fs/fs.h>

#include <arch/spinlock.h>
#include <arch/fake_task.h>
#include <arch/kernel_task.h>
#include <arch/drivers/pic8259_timer.h>
#include <arch/idle_task.h>
#include <arch/context.h>
#include <arch/backtrace.h>
#include <arch/gdt.h>
#include <arch/idt.h>
#include <arch/int.h>
#include "irq_handler.h"
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

void task_switch(context_t *old, struct task *new)
{
    cpu_set_kernel_stack(cpu_get_local(), new->kstack_top);
    set_current_page_directory(V2P(new->page_dir));

    arch_context_switch(&new->context, old);
}

static char *task_setup_scheduler_entry(struct task *t, char *ksp)
{
    ksp -= sizeof(*t->context.esp);
    t->context.esp = (struct x86_regs *)ksp;

    memset(t->context.esp, 0, sizeof(*t->context.esp));
    t->context.esp->eip = (uintptr_t)scheduler_task_entry;

    return ksp;
}

static void task_user_kernel_stack_setup(struct task *t)
{
    char *sp;

    sp = t->kstack_top;

    sp -= sizeof(*t->context.frame);
    t->context.frame = (struct idt_frame *)sp;

    sp -= sizeof(uintptr_t);
    *(uintptr_t *)sp = (uintptr_t)irq_handler_end;

    sp = task_setup_scheduler_entry(t, sp);
}

/* Makes a copy the pages in the user memory space in one task to another
 * task */
void task_paging_copy_user(struct task *restrict new, struct task *restrict old)
{
    struct page_directory *restrict new_dir = new->page_dir;
    struct page_directory *restrict old_dir = old->page_dir;
    int table;

    if (old->kernel)
        return ;

    kprintf("Copying user pages\n");

    for (table = 0; table < KMEM_KPAGE; table++) {
        if (old_dir->table[table].present) {
            struct page_table *restrict new_table;
            struct page_table *restrict old_table = (struct page_table *)P2V(PAGING_FRAME(old_dir->table[table].entry));

            pa_t flags = old_dir->table[table].entry & PAGING_ATTRS_MASK;
            pa_t new_table_addr = palloc_phys(PAL_KERNEL) | flags;

            /* Insert new page into the new page-table */
            new_dir->table[table].entry = new_table_addr;

            new_table = (struct page_table *)P2V(PAGING_FRAME(new_table_addr));

            /* Make copies of every page the old task has, and place them at the same addresses for the new task */
            int page;
            for (page = 0; page < PG_SIZE / 4; page++) {
                if (old_table->table[page].present) {

                    pa_t flags = old_table->table[page].entry & PAGING_ATTRS_MASK;
                    pa_t new_page = palloc_phys(PAL_KERNEL);

                    memcpy(P2V(new_page), P2V(PAGING_FRAME(old_table->table[page].entry)), PG_SIZE);
                    new_table->table[page].entry = new_page | flags;
                } else {
                    new_table->table[page].entry = 0;
                }
            }
        }
    }
}

void task_paging_init(struct task *task)
{
    if (!task->kernel) {
        task->page_dir = palloc(PAL_KERNEL);
        memcpy(task->page_dir, &kernel_dir, sizeof(kernel_dir));
    } else {
        task->page_dir = &kernel_dir;
    }
}

void task_paging_free(struct task *task)
{
    int table, page;

    if (task->kernel) {
        task->page_dir = NULL;
        return ;
    }

    for (table= 0; table < PAGING_DIR_INDEX(KMEM_KBASE); table++) {
        if (task->page_dir->table[table].present) {
            struct page_table *t = P2V(PAGING_FRAME(task->page_dir->table[table].entry));
            for (page = 0; page < PG_SIZE / 4; page++)
                if (t->table[page].present)
                    pfree_phys(PAGING_FRAME(t->table[page].entry));

            pfree(t);
        }
    }

    pfree(task->page_dir);
    task->page_dir = NULL;
}

static struct task *task_kernel_generic(struct task *t, char *name, int (*kernel_task)(void *), void *ptr, uintptr_t task_entry)
{
    char *ksp;

    t->kernel = 1;
    task_paging_init(t);

    strcpy(t->name, name);

    /* We never exit to user-mode, so we have no frame */
    t->context.frame = NULL;

    ksp = t->kstack_top;

    /* Push ptr onto the stack, as an argument for 'kernel_task' */
    ksp -= sizeof(ptr);
    *(void **)ksp = ptr;

    /* Push the address of 'kernel_task'. The task_entry function will pop it
     * off the stack and call it for us */
    ksp -= sizeof(kernel_task);
    *(void **)ksp = kernel_task;

    /* Entry point for kernel tasks. We push this address below the
     * scheduler_entry, so that when that function returns, it will "return" to
     * the task_entry function */
    ksp -= sizeof(uintptr_t);
    *(uintptr_t *)ksp = (uintptr_t)task_entry;

    task_setup_scheduler_entry(t, ksp);

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

    return task_kernel_generic(t, name, kernel_task, ptr, kernel_task_entry_interruptable_addr);
}

struct task *task_kernel_new(char *name, int (*kernel_task)(void *), void *ptr)
{
    struct task *t = task_new();

    if (!t)
        return NULL;

    return task_kernel_generic(t, name, kernel_task, ptr, kernel_task_entry_addr);
}

void task_init(struct task *task)
{
    memset(task, 0, sizeof(*task));

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

    task_paging_init(new);
    task_paging_copy_user(new, parent);
    task_user_kernel_stack_setup(new);

    new->cwd = inode_dup(parent->cwd);

    for (i = 0; i < NOFILE; i++)
        new->files[i] = file_dup(parent->files[i]);

    new->parent = parent;
    memcpy(new->context.frame, parent->context.frame, sizeof(*new->context.frame));

    return new;
}

struct task *task_fake_create(void)
{
    pa_t code, sp;
    va_t code_load_addr = KMEM_PROG_LINK;
    va_t stack_load_addr = (va_t)KMEM_PROG_STACK_START;

    struct task *t = task_new();

    task_paging_init(t);

    snprintf(t->name, sizeof(t->name), "Fake Task %d", t->pid);

    code = palloc_phys(PAL_KERNEL);
    memcpy(P2V(code), fake_task_entry, fake_task_size);
    paging_map_phys_to_virt(V2P(t->page_dir), code_load_addr, code);

    memset(&t->context, 0, sizeof(t->context));

    sp = palloc_phys_multiple(PAL_KERNEL, log2(KMEM_STACK_LIMIT));
    paging_map_phys_to_virt_multiple(V2P(t->page_dir), stack_load_addr, sp, KMEM_STACK_LIMIT);

    /* Setup the stack */
    task_user_kernel_stack_setup(t);

    t->context.frame->eax = 'a' + t->pid - 1;

    t->context.frame->ds = _USER_DS | DPL_USER;
    t->context.frame->es = t->context.frame->ds;

    t->context.frame->cs = _USER_CS | DPL_USER;
    t->context.frame->eip = (uintptr_t)code_load_addr;

    t->context.frame->ss = t->context.frame->ds;
    t->context.frame->esp = (uintptr_t)KMEM_PROG_STACK_END - 1;

    t->context.frame->eflags = EFLAGS_IF;

    t->state = TASK_RUNNABLE;

    t->cwd = inode_dup(ino_root);

    return t;
}

void task_free(struct task *t)
{
    task_paging_free(t);

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

