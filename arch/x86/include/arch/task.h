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

struct i387_fxsave {
    uint16_t cwd; /* x87 FPU Control Word */
    uint16_t swd; /* x87 FPU Status Word */

    uint8_t twd; /* x87 FPU Tag Word */
    uint8_t rsvd1;

    uint16_t fop; /* x87 FPU Opcode */
    uint32_t fip; /* x87 FPU IP Offset */
    uint16_t fcs; /* x87 FPU IP Selector */
    uint16_t rsvd2;

    uint32_t fdp; /* x87 FPU Instruction Operand Offset */
    uint16_t fds; /* x87 FPU Instruction Operand Selector */
    uint16_t rsvd3;

    uint32_t mxcsr;
    uint32_t mxcsr_mask;
    uint32_t st_space[32];
    uint32_t xmm_space[64];
    uint32_t padding[24];
} __align(16);

static inline void i387_fxsave(struct i387_fxsave *fxsave)
{
    asm volatile("fxsave (%0)":: "r" (fxsave));
}

static inline void i387_fxrstor(struct i387_fxsave *fxsave)
{
    asm volatile("fxrstor (%0)":: "r" (fxsave));
}

struct arch_task_info {
    struct i387_fxsave fxsave;
};

#define ARCH_TASK_INFO_INIT() \
    { \
        .fxsave.cwd = 0x37F, \
        .fxsave.mxcsr = 0x1F80, \
    }

static inline void arch_task_info_init(struct arch_task_info *info)
{
    *info = (struct arch_task_info)ARCH_TASK_INFO_INIT();
}

/* Note, you should probably be calling the functions in vm.h unless you know
 * what you're doing. */

/* 'setup_stack_user()' takes a string pointing to the executable to start.
 * This is required because 'exec()' is called to insert the executable image
 * for the userspace task, and this call has to be setup on the stack. */
void arch_task_setup_stack_user(struct task *t);
void arch_task_setup_stack_user_with_exec(struct task *t, const char *exe);
void arch_task_setup_stack_kernel(struct task *t, int (*kernel_task) (void *), void *ptr);
void arch_task_setup_stack_kernel_interruptable(struct task *t, int (*kernel_task) (void *), void *ptr);

void arch_task_switch(context_t *old, struct task *new);

void arch_task_init(struct task *t);

extern uintptr_t arch_task_user_entry_addr;

#define task_switch(old, new) arch_task_switch(old, new)

#define arch_address_space_switch_to_kernel() \
    set_current_page_directory(V2P(&kernel_dir))

#endif
