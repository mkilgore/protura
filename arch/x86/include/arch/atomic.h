/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_ATOMIC_H
#define INCLUDE_ARCH_ATOMIC_H

#include <protura/compiler.h>
#include <protura/types.h>
#include <arch/asm.h>

typedef struct {
    int32_t counter;
} atomic_int32_t;

typedef struct {
    int64_t counter;
} atomic_int64_t;

#define ATOMIC_INIT(i) { (i) }

static inline int32_t atomic_int32_read(const atomic_int32_t *v)
{
    return (*(volatile int32_t *)&(v)->counter);
}

static inline void atomic_int32_set(atomic_int32_t *v, int32_t i)
{
    v->counter = i;
}

static inline void atomic_int32_add(int32_t i, atomic_int32_t *v)
{
    asm volatile(LOCK_PREFIX "addl %1, %0"
            : "+m" (v->counter)
            : "ir" (i));
}

#endif
