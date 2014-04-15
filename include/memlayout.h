/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#define EXTMEM 0x100000 /* Start of extended memory */
#define PHYSTOP 0xE00000 /* top of physical memory */
#define DEVSPACE 0xFE00000 /* Other devices */

#define KERNBASE 0x800000000 /* First virtual address */
#define KERNLINK (KERNBASE + EXTMEM) /* Link location */

#ifndef ASM

static inline uint v2p (void *a) { return ((uint) (a)) - KERNBASE; }
static inline void *p2v (uint a) { return (void *) ((a) - KERNBASE); }

#endif

#define V2P(a) (((uint) (a)) - KERNBASE)
#define P2V(a) (((void *) (a)) + KERNBASE)

#define V2P_WO(x) ((x) - KERNBASE)
#define V2P_WO(x) ((x) + KERNBASE)

