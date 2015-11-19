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
#include <mm/vm.h>
#include <drivers/term.h>
#include <protura/dump_mem.h>
#include <protura/scheduler.h>
#include <protura/task.h>
#include <mm/palloc.h>
#include <fs/inode.h>
#include <fs/file.h>
#include <fs/fs.h>
#include <fs/sys.h>

#include <arch/spinlock.h>
#include <arch/fake_task.h>
#include <arch/kernel_task.h>
#include <arch/context.h>
#include <arch/backtrace.h>
#include <arch/gdt.h>
#include <protura/irq.h>
#include <arch/int.h>
#include <arch/idt.h>
#include "irq_handler.h"
#include <arch/paging.h>
#include <arch/asm.h>
#include <arch/cpu.h>
#include <arch/task.h>

void arch_task_switch(context_t *old, struct task *new)
{
    cpu_set_kernel_stack(cpu_get_local(), new->kstack_top);

    if (new->kernel)
        set_current_page_directory(V2P(&kernel_dir));
    else
        set_current_page_directory(V2P(new->addrspc->page_dir));

    arch_context_switch(&new->context, old);
}

static char *setup_scheduler_entry(struct task *t, char *ksp)
{
    ksp -= sizeof(*t->context.esp);
    t->context.esp = (struct x86_regs *)ksp;

    memset(t->context.esp, 0, sizeof(*t->context.esp));
    t->context.esp->eip = (uintptr_t)scheduler_task_entry;

    return ksp;
}

void arch_task_setup_stack_user_with_exec(struct task *t, const char *exe)
{
    char *ksp = t->kstack_top;

    ksp -= sizeof(*t->context.frame);
    t->context.frame = (struct irq_frame *)ksp;
    memset(t->context.frame, 0, sizeof(*t->context.frame));

    ksp -= sizeof(uintptr_t);
    *(uintptr_t *)ksp = (uintptr_t)irq_handler_end;

    if (exe) {
        /* Note that this code depends on the code at arch_task_user_entry.
         * Notably, we push the arguments for sys_exec onto the stack here, and
         * arch_task_user_entry pops them off. */
        ksp -= sizeof(struct irq_frame *);
        *(struct irq_frame **)ksp = t->context.frame;

        ksp -= sizeof(char *);
        *(void **)ksp = NULL;

        ksp -= sizeof(exe);
        *(const char **)ksp = exe;

        ksp -= sizeof(uintptr_t);
        *(uintptr_t *)ksp = arch_task_user_entry_addr;
    }

    ksp = setup_scheduler_entry(t, ksp);
}

void arch_task_setup_stack_user(struct task *t)
{
    arch_task_setup_stack_user_with_exec(t, NULL);
}

static void setup_kernel_stack_generic(struct task *t, int (*kernel_task) (void *), void *ptr, uintptr_t task_entry)
{
    char *ksp = t->kstack_top;

    /* Push ptr onto stack to use as argument for 'kernel_task'. Then push
     * address of kernel_task to run. The function is run when 'task_entry'
     * pops the address of 'kernel_task' off of the stack when it returns. */
    ksp -= sizeof(ptr);
    *(void **)ksp = ptr;

    ksp -= sizeof(kernel_task);
    *(void **)ksp = kernel_task;

    /* Entry point for kernel tasks. We push this address below the
     * scheduler_entry, so that when that function returns, it will "return" to
     * the task_entry function */
    ksp -= sizeof(uintptr_t);
    *(uintptr_t *)ksp = (uintptr_t)task_entry;

    ksp = setup_scheduler_entry(t, ksp);

    return ;
}

void arch_task_setup_stack_kernel(struct task *t, int (*kernel_task) (void *), void *ptr)
{
    setup_kernel_stack_generic(t, kernel_task, ptr, kernel_task_entry_addr);
}

void arch_task_setup_stack_kernel_interruptable(struct task *t, int (*kernel_task) (void *), void *ptr)
{
    setup_kernel_stack_generic(t, kernel_task, ptr, kernel_task_entry_interruptable_addr);
}

void arch_task_change_address_space(struct address_space *addrspc)
{
    struct task *current = cpu_get_local()->current;
    struct address_space *old = current->addrspc;

    current->addrspc = addrspc;
    set_current_page_directory(V2P(addrspc->page_dir));

    address_space_clear(old);
    kfree(old);
}

