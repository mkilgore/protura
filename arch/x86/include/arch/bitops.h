/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_BITOPS_H
#define INCLUDE_ARCH_BITOPS_H

#include <protura/compiler.h>
#include <arch/asm.h>

/* Atomic bit-twaddles */
static __always_inline void set_bit(volatile unsigned int *value, int bit)
{
    asm volatile(LOCK_PREFIX "bts %1, %0"
            : "+m" (*(volatile int *)value)
            : "Ir" (bit)
            : "memory");
}

static __always_inline void clear_bit(volatile unsigned int *value, int bit)
{
    asm volatile(LOCK_PREFIX "btr %1, %0"
            : "+m" (*(volatile int *)value)
            : "Ir" (bit)
            : "memory");
}

static __always_inline int test_bit(const volatile unsigned int *value, int bit)
{
    int old;

    asm volatile("bt %2, %1\n"
                 "sbb %0, %0"
                 : "=r" (old)
                 : "m" (*(unsigned int *)value), "Ir" (bit));

    return old;
}

#endif
