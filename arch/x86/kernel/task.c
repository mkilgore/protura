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

void task_init(void)
{

}

const char *task_states[] = {
    [TASK_NONE]     = "no state",
    [TASK_SLEEPING] = "sleeping",
    [TASK_RUNNABLE] = "runnable",
    [TASK_RUNNING]  = "running",
};

void task_print(char *buf, ksize_t size, struct task *task)
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
    unsigned int palloc_flags = (in_atomic())? PAL_ATOMIC: PAL_KERNEL;

    kprintf("Copying user pages\n");

    for (table = 0; table < KMEM_KPAGE; table++) {
        if (old_dir->table[table].present) {
            struct page_table *restrict new_table;
            struct page_table *restrict old_table = (struct page_table *)P2V(PAGING_FRAME(old_dir->table[table].entry));

            pa_t flags = old_dir->table[table].entry & PAGING_ATTRS_MASK;
            pa_t new_table_addr = palloc_phys(palloc_flags) | flags;

            /* Insert new page into the new page-table */
            new_dir->table[table].entry = new_table_addr;

            new_table = (struct page_table *)P2V(PAGING_FRAME(new_table_addr));

            /* Make copies of every page the old task has, and place them at the same addresses for the new task */
            int page;
            for (page = 0; page < PG_SIZE / 4; page++) {
                if (old_table->table[page].present) {

                    pa_t flags = old_table->table[page].entry & PAGING_ATTRS_MASK;
                    pa_t new_page = palloc_phys(palloc_flags);

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
    unsigned int palloc_flags = (in_atomic())? PAL_ATOMIC: PAL_KERNEL;
    task->page_dir = palloc(palloc_flags);
    memcpy(task->page_dir, &kernel_dir, sizeof(kernel_dir));
}

void task_paging_free(struct task *task)
{
    int table, page;

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

static struct task *task_kernel_generic(char *name, int (*kernel_task)(int argc, const char **argv), int argc, const char **argv, uintptr_t task_entry)
{
    char *ksp;
    struct task *t = task_new();

    if (!t)
        return NULL;

    strcpy(t->name, name);

    /* We never exit to user-mode, so we have no frame */
    t->context.frame = NULL;

    ksp = t->kstack_top;

    /* Push argv and argc onto the stack, as arguments for 'kernel_task' */
    ksp -= sizeof(argv);
    *(void **)ksp = argv;

    ksp -= sizeof(argc);
    *(int *)ksp = argc;

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
struct task *task_kernel_new_interruptable(char *name, int (*kernel_task)(int argc, const char **argv), int argc, const char **argv)
{
    return task_kernel_generic(name, kernel_task, argc, argv, kernel_task_entry_interruptable_addr);
}

struct task *task_kernel_new(char *name, int (*kernel_task)(int argc, const char **argv), int argc, const char **argv)
{
    return task_kernel_generic(name, kernel_task, argc, argv, kernel_task_entry_addr);
}

/* Initializes a new allocated task */
struct task *task_new(void)
{
    unsigned int palloc_flags = (in_atomic())? PAL_ATOMIC: PAL_KERNEL;
    struct task *task;

    task = kmalloc(sizeof(*task), palloc_flags);
    if (!task)
        return NULL;

    memset(task, 0, sizeof(*task));

    task->pid = scheduler_next_pid();

    task->kstack_bot = palloc_multiple(palloc_flags, log2(KMEM_STACK_LIMIT));
    task->kstack_top = task->kstack_bot + PG_SIZE * KMEM_STACK_LIMIT - 1;

    task_paging_init(task);
    task->state = TASK_RUNNABLE;

    kprintf("Created task %d\n", task->pid);

    return task;
}

/* Creates a new processes that is a copy of 'parent'.
 * Note, the userspace code/data/etc. is copied, but not the kernel-space stuff
 * like the kernel stack. */
struct task *task_fork(struct task *parent)
{
    struct task *new = task_new();
    if (!new)
        return NULL;

    snprintf(new->name, sizeof(new->name), "Chld: %s", parent->name);

    task_paging_copy_user(new, parent);
    task_user_kernel_stack_setup(new);

    new->parent = parent;
    memcpy(new->context.frame, parent->context.frame, sizeof(*new->context.frame));

    return new;
}

struct task *task_fake_create(void)
{
    unsigned int palloc_flags = (in_atomic())? PAL_ATOMIC: PAL_KERNEL;
    pa_t code, sp;
    va_t code_load_addr = KMEM_PROG_LINK;
    va_t stack_load_addr = (va_t)KMEM_PROG_STACK_START;

    struct task *t = task_new();

    snprintf(t->name, sizeof(t->name), "Fake Task %d", t->pid);

    code = palloc_phys(palloc_flags);
    memcpy(P2V(code), fake_task_entry, fake_task_size);
    paging_map_phys_to_virt(V2P(t->page_dir), code_load_addr, code);

    memset(&t->context, 0, sizeof(t->context));

    sp = palloc_phys_multiple(palloc_flags, log2(KMEM_STACK_LIMIT));
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

    return t;
}

void task_free(struct task *t)
{
    task_paging_free(t);

    pfree_multiple(t->kstack_bot, log2(KERNEL_STACK_PAGES));

    kfree(t);
}

