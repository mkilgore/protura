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

typedef unsigned int flags_t;

/* Atomic bit-twaddles */
static __always_inline void bit_set(volatile unsigned int *value, int bit)
{
    asm volatile(LOCK_PREFIX "bts %1, %0"
            : "+m" (*(volatile int *)value)
            : "Ir" (bit)
            : "memory");
}

static __always_inline void bit_clear(volatile unsigned int *value, int bit)
{
    asm volatile(LOCK_PREFIX "btr %1, %0"
            : "+m" (*(volatile int *)value)
            : "Ir" (bit)
            : "memory");
}

static __always_inline int bit_test(const volatile unsigned int *value, int bit)
{
    int old;

    asm volatile("bt %2, %1\n"
                 "sbb %0, %0"
                 : "=r" (old)
                 : "m" (*(unsigned int *)value), "Ir" (bit));

    return old;
}

static __always_inline int bit_test_and_set(const volatile unsigned int *value, int bit)
{
    int old;

    asm volatile(LOCK_PREFIX "bts %2, %0;"
            "movl %1, $0;"
            "jc f1;"
            "movl %1, $1;"
            "1:"
            : "+m" (*(volatile int *)value), "=r" (old)
            : "Ir" (bit)
            : "memory");

    return old;
}

#define flag_set(flags, f) bit_set(flags, f)
#define flag_clear(flags, f) bit_clear(flags, f)
#define flag_test(flags, f) bit_test(flags, f)
#define flag_test_and_set(flags, f) bit_test_and_set(flags, f)

#endif
