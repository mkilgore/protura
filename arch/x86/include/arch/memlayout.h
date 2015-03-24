/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_MEMLAYOUT_H
#define INCLUDE_ARCH_MEMLAYOUT_H

#include <arch/paging.h>

#define KMEM_EXTMEM   0x00100000
#define KMEM_KBASE    0xC0000000
#define KMEM_DEVSPACE 0xFE000000

/* This size is the ammount of memory that the kernel should be mapped in for
 * the initial booting (Until we having our actual paging and memory manager
 * going) */
/* 16 MB's */
#define KMEM_KERNEL_SIZE 0x01000000

#define KMEM_LINK (KMEM_KBASE + KMEM_EXTMEM)
#define KMEM_KPAGE  PAGING_DIR_INDEX(KMEM_KBASE)

#define V2P(x) (((uintptr_t) (x) - KMEM_KBASE))
#define P2V(x) ((void *)((uintptr_t) (x) + KMEM_KBASE))

#define V2P_WO(x) ((x) - KMEM_KBASE)
#define P2V_WO(x) ((x) + KMEM_KBASE)

/* In pages - 8MB */
#define KMEM_STACK_LIMIT     4000

/* The start and end of a program's stack, in virtual memory. The stack goes
 * from KMEM_PROG_STACK_START TO (KMEM_PROG_STACK_END - 1) */
#define KMEM_PROG_STACK_END (void *)KMEM_KBASE
#define KMEM_PROG_STACK_START (void *)(KMEM_PROG_STACK_END - KMEM_STACK_LIMIT * PG_SIZE)

/* Location that a program's data will be loaded into */
#define KMEM_PROG_LINK (void *)0x08048000

#endif
