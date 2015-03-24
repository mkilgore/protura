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
#include <arch/idle_task.h>
#include <arch/context.h>
#include <arch/gdt.h>
#include "irq_handler.h"
#include <arch/paging.h>
#include <arch/asm.h>
#include <arch/cpu.h>
#include <arch/task.h>


static struct {
    struct spinlock lock;
    struct list_head list;

    struct list_head empty;

    struct arch_context *scheduler;

    pid_t next_pid;
} ktasks = {
    .lock = SPINLOCK_INIT("Task list lock"),
    .list = LIST_HEAD_INIT(ktasks.list),
    .empty = LIST_HEAD_INIT(ktasks.empty),
    .scheduler = NULL,
    .next_pid = 0,
};

#define KERNEL_STACK_PAGES 2

void task_init(void)
{

}

/* This is a child's very first entry-point from the scheduler. */
static void new_task_ret(void)
{
    /* Necessary to release lock, because scheduler acquires it before switching
     * contexts. */
    spinlock_release(&ktasks.lock);

    /* task_new() set's it up to we 'return' to 'irq_handler_end', which will
     * iret us into the user-code */
}

void task_add(struct task *task)
{
    /* Add this task to the beginning of the list of current tasks.
     * Adding to the beginning means it will get the next time-slice. */
    using_spinlock(&ktasks.lock)
        list_add(&ktasks.list, &task->task_list_node);
}

void task_remove(struct task *task)
{
    /* Remove 'task' from the list of tasks to schedule. */
    using_spinlock(&ktasks.lock)
        list_del(&task->task_list_node);
}

void task_yield(void)
{
    using_spinlock(&ktasks.lock) {
        /* If ktasks.scheduler is NULL, then we're already in the scheduler, so we
         * can't yield. */
        if (ktasks.scheduler && curcpu->current) {
            curcpu->current->state = TASK_RUNNABLE;
            arch_context_switch(&curcpu->current->context, ktasks.scheduler);
        }
    }
}

void scheduler(void)
{
    struct task *t;

    kprintf("scheduler: Staring!\n");

    for (;;) {
        sti(); /* We force interrupts on so that other tasks can get scheduled
                * on other CPU's which need access to ktasks.lock. The
                * spinlock acquire will turn interrupts back off */
        using_spinlock(&ktasks.lock) {
            t = list_first(&ktasks.list, struct task, task_list_node);
            list_rotate_left(&ktasks.list);

            if (t->state != TASK_RUNNABLE)
                continue;

            curcpu->current = t;

            cpu_set_kernel_stack(curcpu, t->kern_stack);
            set_current_page_directory(V2P(t->page_dir));
            arch_context_switch(&ktasks.scheduler, t->context);

            /* When we get here, we just switched into the scheduler. Set it to
             * NULL so nobody else can switch into the scheduler while we're here
             * 
             * This is a flag to indicate that the scheduler is currently running,
             * so nobody else should attempt to schedule (And this is enforced by
             * the fact that ktasks.scheduler is set to NULL, so any accidental
             * attempt to call the scheduler will result in a NULL deref and a
             * page-fault). */
            ktasks.scheduler = NULL;
            set_current_page_directory(V2P(&kernel_dir));

            curcpu->current = NULL;
        }
    }
}

struct task *task_new(char *name)
{
    struct task *task;
    char *kstack;

    task = kmalloc(sizeof(*task), PMAL_KERNEL);
    if (!task)
        return NULL;

    memset(task, 0, sizeof(*task));

    strcpy(task->name, name);
    task->state = TASK_EMBRYO;
    task->pid = ktasks.next_pid++;

    task->kern_stack = P2V(pmalloc_pages_alloc(PMAL_KERNEL, KERNEL_STACK_PAGES)) + KERNEL_STACK_PAGES * PG_SIZE - 1;
    kstack = task->kern_stack;

    /* Put the idt_frame on the bottom of the stack */
    kstack -= sizeof(*task->frame);
    task->frame = (struct idt_frame *)kstack;

    /* Put the address 'irq_handler_end' on the stack, so 'new_task_ret' returns to it */
    kstack -= 4;
    *(uint32_t *)(kstack) = (uint32_t)irq_handler_end;

    /* Setup the context for use by arch_switch_context. The context acts like
     * arch_switch_context was called from inside of 'new_task_ret', so
     * 'new_task_ret' is run when arch_switch_context is called with this
     * tasks's context and arch_switch_context returns. */
    kstack -= sizeof(*task->context);
    task->context = (struct arch_context *)kstack;

    memset(task->context, 0, sizeof(*task->context) - 1);
    task->context->eip = (uint32_t)new_task_ret;

    task_paging_init(task);

    return task;
}

struct task *task_fork(struct task *parent)
{
    struct task *new = task_new(parent->name);
    if (!new)
        return NULL;

    task_paging_copy_user(new, parent);

    new->parent = parent;
    *new->frame = *parent->frame;

    return new;
}

void task_paging_init(struct task *task)
{
    task->page_dir = P2V(pmalloc_page_alloc(PMAL_KERNEL));
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
                    pmalloc_page_free(PAGING_FRAME(t->table[page].entry));

            pmalloc_page_free(V2P(t));
        }
    }

    pmalloc_page_free(V2P(task->page_dir));
    task->page_dir = NULL;
}

void task_fake_create(void)
{
    pa_t code, sp;
    va_t code_load_addr = KMEM_PROG_LINK;
    va_t stack_load_addr = (va_t)KMEM_PROG_STACK_START;
    struct task *t = task_new("Fake Task");

    code = pmalloc_page_alloc(PMAL_KERNEL);
    memcpy(P2V(code), fake_task_entry, fake_task_size);
    paging_map_phys_to_virt(V2P(t->page_dir), code_load_addr, code);

    memset(t->frame, 0, sizeof(*t->frame));

    t->frame->eax = t->pid + 'a';
    t->frame->cs = _USER_CS | DPL_USER;
    t->frame->ds = _USER_DS | DPL_USER;
    t->frame->es = t->frame->ds;
    t->frame->ss = t->frame->ds;
    t->frame->eflags = EFLAGS_IF;
    t->frame->eip = (uintptr_t)code_load_addr;

    sp = pmalloc_pages_alloc(PMAL_KERNEL, KMEM_STACK_LIMIT);
    paging_map_phys_to_virt_multiple(V2P(t->page_dir), stack_load_addr, sp, KMEM_STACK_LIMIT);

    t->frame->esp = (uintptr_t)KMEM_PROG_STACK_END - 1;

    t->state = TASK_RUNNABLE;
    task_add(t);
}

void task_paging_copy_user(struct task *restrict new, struct task *restrict old)
{
    struct page_directory *restrict new_dir = new->page_dir;
    struct page_directory *restrict old_dir = old->page_dir;
    int table;

    for (table = 0; table < PAGING_DIR_INDEX(KMEM_KBASE); table++) {
        if (old_dir->table[table].present) {
            pa_t flags = old_dir->table[table].entry & PAGING_ATTRS_MASK;
            pa_t new_table = pmalloc_page_alloc(PMAL_KERNEL) | flags;

            /* Insert new page into the new page-table */
            new_dir->table[table].entry = new_table;

            /* Just memcpy the table, since we're going to map the same
             * addresses */
            memcpy(P2V(PAGING_FRAME(new_table)), P2V(PAGING_FRAME(old_dir->table[table].entry)), PG_SIZE);
        }
    }
}

static const char *task_states[] = {
    [TASK_UNUSED]   = "unused",
    [TASK_EMBRYO]   = "embryo",
    [TASK_SLEEPING] = "sleeping",
    [TASK_RUNNABLE] = "runnable",
    [TASK_RUNNING]  = "running",
    [TASK_ZOMBIE]   = "zombie"
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

