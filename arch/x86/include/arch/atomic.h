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
} atomic32_t;

#define ATOMIC32_INIT(i) { (i) }

static __always_inline int32_t atomic32_get(const atomic32_t *v)
{
    return (*(volatile int32_t *)&(v)->counter);
}

static __always_inline void atomic32_set(atomic32_t *v, int32_t i)
{
    *(volatile int32_t *)&v->counter = i;
}

static __always_inline void atomic32_add(atomic32_t *v, int32_t i)
{
    asm volatile(LOCK_PREFIX "addl %1, %0"
            : "+m" (v->counter)
            : "ir" (i));
}

static __always_inline void atomic32_sub(atomic32_t *v, int32_t i)
{
    asm volatile(LOCK_PREFIX "subl %1, %0"
            : "+m" (v->counter)
            : "ir" (i));
}

static __always_inline void atomic32_inc(atomic32_t *v)
{
    asm volatile(LOCK_PREFIX "incl %0"
            : "+m" (v->counter));
}

static __always_inline void atomic32_dec(atomic32_t *v)
{
    asm volatile(LOCK_PREFIX "decl %0"
            : "+m" (v->counter));
}

static __always_inline int atomic32_dec_and_test(atomic32_t *v)
{
    asm volatile goto (LOCK_PREFIX "decl %0; jz %l[equal]"
            :
            : "m" (v->counter)
            : "memory"
            : equal);

    return 0;
  equal:
    return 1;
}

typedef atomic32_t atomic_t;

#define atomic_get(a)          atomic32_get(a)
#define atomic_set(a, i)       atomic32_set(a, i)
#define atomic_add(a, i)       atomic32_add(a, i)
#define atomic_sub(a, i)       atomic32_sub(a, i)
#define atomic_inc(a)          atomic32_inc(a)
#define atomic_dec(a)          atomic32_dec(a)
#define atomic_dec_and_test(a) atomic32_dec_and_test(a)

#endif
