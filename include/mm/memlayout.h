/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_MM_MEMLAYOUT_H
#define INCLUDE_MM_MEMLAYOUT_H

#define KMEM_EXTMEM   0x00100000
#define KMEM_KBASE    0xC0000000
#define KMEM_DEVSPACE 0xFE000000

/* 16 MB's */
#define KMEM_LOW_SIZE 0x01000000

#define KMEM_LINK (KMEM_KBASE + KMEM_EXTMEM)

#define KMEM_KPAGE  (KMEM_KBASE >> 22)

#define KMEM_KTABLES (KMEM_LOW_SIZE >> 22)

#define V2P(x) (((uintptr_t) (x) - KMEM_KBASE))
#define P2V(x) ((void *)((uintptr_t) (x) + KMEM_KBASE))

#define V2P_WO(x) ((x) - KMEM_KBASE)
#define P2V_WO(x) ((x) + KMEM_KBASE)

#endif
