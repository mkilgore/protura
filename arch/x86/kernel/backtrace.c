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

#include <arch/backtrace.h>

void dump_stack_ptr(void *start)
{
    struct stackframe *stack = start;
    int frame = 0;

    kp(KP_ERROR, "  Stack: %p\n", start);

    for (frame = 1; stack != 0 && (uintptr_t)stack > 0xC0000000UL; stack = stack->caller_stackframe, frame++)
        kp(KP_ERROR, "  [%d][0x%08x]\n", frame, stack->return_addr);
}

void dump_stack(void)
{
    dump_stack_ptr(read_ebp());
}

