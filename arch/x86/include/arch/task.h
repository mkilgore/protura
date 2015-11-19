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
#include <protura/bits.h>
#include <arch/context.h>
#include <arch/paging.h>
#include <arch/cpu.h>

struct task;
struct address_space;
struct vm_map;

/* Note, you should probably be calling the functions in vm.h unless you know
 * what you're doing. */

/* 'setup_stack_user()' takes a string pointing to the executable to start.
 * This is required because 'exec()' is called to insert the executable image
 * for the userspace task, and this call has to be setup on the stack. */
void arch_task_setup_stack_user(struct task *t);
void arch_task_setup_stack_user_with_exec(struct task *t, const char *exe);
void arch_task_setup_stack_kernel(struct task *t, int (*kernel_task) (void *), void *ptr);
void arch_task_setup_stack_kernel_interruptable(struct task *t, int (*kernel_task) (void *), void *ptr);

/* Takes care of mapping entries into the supplied address-space's page-directory */
void arch_address_space_map_vm_entry(struct address_space *, va_t virtual, pa_t physical, flags_t vm_flags);
void arch_address_space_map_vm_map(struct address_space *, struct vm_map *map);
void arch_address_space_unmap_vm_entry(struct address_space *, va_t addr);
void arch_address_space_unmap_vm_map(struct address_space *, struct vm_map *map);

/* creates a complete copy of an address space, including a completely setup
 * page-directory and a copy of every vm_map. */
void arch_address_space_copy(struct address_space *new, struct address_space *old);

/* Initalizes or releases all memory being used by this address_space.
 *
 * Note that it is necessary to unmap any currently mapped 'vm_map's  before
 * calling 'clear', they are not unmapped automatically.
 */
void arch_address_space_init(struct address_space *addrspc);
void arch_address_space_clear(struct address_space *addrspc);

/* Note, acts on the current task - Changes current task's address_space to be
 * 'new', and also reloads the page-directory at the same time. */
void arch_task_change_address_space(struct address_space *new);

void arch_task_switch(context_t *old, struct task *new);

extern uintptr_t arch_task_user_entry_addr;

#define task_switch(old, new) arch_task_switch(old, new)

#define arch_address_space_switch_to_kernel() \
    set_current_page_directory(V2P(&kernel_dir))

#endif
