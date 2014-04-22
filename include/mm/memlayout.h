/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_MM_MEMLAYOUT_H
#define INCLUDE_MM_MEMLAYOUT_H

#define KMEM_EXTMEM   0x100000
#define KMEM_PHYSTOP  0xE000000
#define KMEM_KBASE    0x80000000
#define KMEM_DEVSPACE 0xFE000000

#define KMEM_LINK (KMEM_KBASE + KMEM_EXTMEM)

#ifndef ASM

#include <protura/compiler.h>

static __always_inline uintptr_t v2p(void *a)
{
    return ((uintptr_t) (a)) - KMEM_KBASE;
}

static __always_inline void *p2v(uintptr_t a)
{
    return (void *) ((a) - KMEM_KBASE);
}

#endif /* ASM */

#define V2P(x) (((uintptr_t) (a)) - KMEM_KBASE)
#define P2V(x) ((void *)((uintptr_t) (a) + KMEM_KBASE))

#define V2P_WO(x) ((x) - KMEM_KBASE)
#define P2V_WO(x) ((x) + KMEM_KBASE)

#endif
