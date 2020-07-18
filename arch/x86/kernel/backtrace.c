/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/compiler.h>
#include <protura/symbols.h>
#include <protura/mm/ptable.h>
#include <arch/memlayout.h>

#include <arch/backtrace.h>

void dump_stack_ptr(void *start, int log_level)
{
    struct stackframe *stack = start;
    int frame = 0;

    pa_t page_dir = get_current_page_directory();
    pgd_t *pgd = p_to_v(page_dir);

    for (frame = 1; stack != 0; stack = stack->caller_stackframe, frame++) {
        if (!pgd_ptr_is_valid(pgd, stack)) {
            kp(log_level, "  Stack is invalid past this point, was: %p\n", stack);
            return;
        }

        const struct symbol *sym = ksym_lookup(stack->return_addr);

        if (sym)
            kp(log_level, "  [%d][0x%08x] %s\n", frame, stack->return_addr, sym->name);
        else
            kp(log_level, "  [%d][0x%08x]\n", frame, stack->return_addr);
    }
}

void dump_stack(int log_level)
{
    dump_stack_ptr(read_ebp(), log_level);
}

