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
    .next_pid = 0,
};

#define KERNEL_STACK_PAGES 2

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
static void task_fork_start(void)
{
    /* kprintf("Return address: %p\n", (void *)read_return_addr()); */
    spinlock_release(&ktasks.lock);
}

void task_init(void)
{

}

void task_schedule_add(struct task *task)
{
    /* Add this task to the beginning of the list of current tasks.
     * Adding to the beginning means it will get the next time-slice. */
    using_spinlock(&ktasks.lock)
        list_add(&ktasks.list, &task->task_list_node);
}

void task_schedule_remove(struct task *task)
{
    /* Remove 'task' from the list of tasks to schedule. */
    using_spinlock(&ktasks.lock)
        list_del(&task->task_list_node);
}

void task_yield(void)
{
    using_spinlock(&ktasks.lock)
        arch_context_switch(&cpu_get_local()->scheduler, &cpu_get_local()->current->context);
}

void task_sleep(uint32_t mseconds)
{
    struct task *t = cpu_get_local()->current;
    t->state = TASK_SLEEPING;
    t->wake_up = timer_get_ticks() + mseconds * (TIMER_TICKS_PER_SEC / 1000);
    kprintf("Wake-up tick: %d\n", t->wake_up);
    task_yield();
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
        struct task *last_task = list_last(&ktasks.list, struct task, task_list_node);
        uint32_t ticks = timer_get_ticks();

        /* Select the first RUNNABLE task in the schedule list.
         *
         * We use 'last_task' to check if we made a full loop over the list
         * of tasks. If we did then we exit anyway even if we didn't find a
         * RUNNABLE task.
         *
         * We do a rotation and grab the first so that when we actually
         * find a task, the next scheduler to run will grab the task
         * afterward. This hopefully lets all tasks get an equal amount of
         * run time (Assuing they're actually asking for runtime). */
        do {
            t = list_first(&ktasks.list, struct task, task_list_node);
            list_rotate_left(&ktasks.list);

            switch (t->state) {
            case TASK_RUNNABLE:
                goto found_task;

            case TASK_SLEEPING:
                if (t->wake_up <= ticks)
                    goto found_task;
                break;

            case TASK_RUNNING:
                break;
            }
        } while (t != last_task);

        /* We execute this cpu's idle task if we didn't find a task to run */
        t = cpu_get_local()->kidle;

    found_task:

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

struct task *task_new(char *name)
{
    struct task *task;

    task = kmalloc(sizeof(*task), PMAL_KERNEL);
    if (!task)
        return NULL;

    memset(task, 0, sizeof(*task));

    strcpy(task->name, name);
    task->pid = ktasks.next_pid++;

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
    new->context= parent->context;

    return new;
}

void task_paging_init(struct task *task)
{
    task->page_dir = P2V(pmalloc_page_alloc(PMAL_KERNEL));
    kprintf("PAGE DIR: %p\n", task->page_dir);
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

static char *task_setup_fork_ret(struct task *t, char *ksp)
{
    ksp -= sizeof(*t->context.esp);
    t->context.esp = (struct x86_regs *)ksp;

    memset(t->context.esp, 0, sizeof(*t->context.esp));
    t->context.esp->eip = (uintptr_t)task_fork_start;
    kprintf("EIP: %p, ESP: %p\n", (void *)t->context.esp->eip, t->context.esp);

    return ksp;
}

static void task_user_kernel_stack_setup(struct task *t)
{
    char *sp;
    t->kstack_bot = P2V(pmalloc_pages_alloc(PMAL_KERNEL, KERNEL_STACK_PAGES));

    kprintf("Task Kstack: %p\n", t->kstack_bot);

    sp = t->kstack_bot + PG_SIZE * KERNEL_STACK_PAGES - 1;
    t->kstack_top = sp;

    sp -= sizeof(*t->context.frame);
    t->context.frame = (struct idt_frame *)sp;

    sp -= sizeof(uintptr_t);
    *(uintptr_t *)sp = (uintptr_t)irq_handler_end;
    kprintf("Irq handler end: %p\n", irq_handler_end);

    sp = task_setup_fork_ret(t, sp);
}

struct task *task_fake_create(void)
{
    pa_t code, sp;
    va_t code_load_addr = KMEM_PROG_LINK;
    va_t stack_load_addr = (va_t)KMEM_PROG_STACK_START;
    struct task *t = task_new("Fake Task");

    code = pmalloc_page_alloc(PMAL_KERNEL);
    memcpy(P2V(code), fake_task_entry, fake_task_size);
    paging_map_phys_to_virt(V2P(t->page_dir), code_load_addr, code);

    memset(&t->context, 0, sizeof(t->context));

    sp = pmalloc_pages_alloc(PMAL_KERNEL, KMEM_STACK_LIMIT);
    paging_map_phys_to_virt_multiple(V2P(t->page_dir), stack_load_addr, sp, KMEM_STACK_LIMIT);

    /* Setup the stack */
    task_user_kernel_stack_setup(t);

    t->context.frame->eax = 'a' + t->pid;

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

static struct task *task_kernel_generic(char *name, int (*kernel_task)(int argc, const char **argv), int argc, const char **argv, uintptr_t task_entry)
{
    char *ksp, *ksp_bot;
    struct task *t = task_new(name);

    if (!t)
        return NULL;

    /* We never exit to user-mode, so we have no frame */
    t->context.frame = NULL;

    ksp_bot = P2V(pmalloc_pages_alloc(PMAL_KERNEL, KMEM_STACK_LIMIT));
    t->kstack_bot = ksp_bot;

    ksp = ksp_bot + PG_SIZE * KERNEL_STACK_PAGES - 1;

    ksp -= sizeof(argc);
    *(int *)ksp = argc;

    ksp -= sizeof(argv);
    *(void **)ksp = argv;

    ksp -= sizeof(kernel_task);
    *(void **)ksp = kernel_task;

    ksp -= sizeof(uintptr_t);
    *(uintptr_t *)ksp = (uintptr_t)task_entry;

    task_setup_fork_ret(t, ksp);

    t->state = TASK_RUNNABLE;
    return t;
}

struct task *task_kernel_new_interruptable(char *name, int (*kernel_task)(int argc, const char **argv), int argc, const char **argv)
{
    return task_kernel_generic(name, kernel_task, argc, argv, kernel_task_entry_interruptable_addr);
}

struct task *task_kernel_new(char *name, int (*kernel_task)(int argc, const char **argv), int argc, const char **argv)
{
    return task_kernel_generic(name, kernel_task, argc, argv, kernel_task_entry_addr);
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

void task_switch(struct arch_context *old, struct task *new)
{
    cpu_set_kernel_stack(cpu_get_local(), new->kstack_top);
    set_current_page_directory(V2P(new->page_dir));

    arch_context_switch(&new->context, old);
}

