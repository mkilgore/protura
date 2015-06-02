/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_MEMLAYOUT_H
#define INCLUDE_ARCH_MEMLAYOUT_H

#include <config/autoconf.h>
#include <arch/paging.h>

#define KMEM_EXTMEM   CONFIG_KERNEL_EXTMEM
#define KMEM_KBASE    CONFIG_KERNEL_BASE

/* This size is the ammount of memory that the kernel should be mapped in for
 * the initial booting (Until we having our actual paging and memory manager
 * going) */
#define KMEM_KERNEL_SIZE CONFIG_KERNEL_SIZE

#define KMEM_KERNEL_END  (KMEM_KBASE + KMEM_KERNEL_SIZE)

#define KMEM_LINK (KMEM_KBASE + KMEM_EXTMEM)
#define KMEM_KPAGE  PAGING_DIR_INDEX(KMEM_KBASE)
#define KMEM_KPAGE_END PAGING_DIR_INDEX(KMEM_KERNEL_END)

#define V2P(x) (((uintptr_t) (x) - KMEM_KBASE))
#define P2V(x) ((void *)((uintptr_t) (x) + KMEM_KBASE))

#define V2P_WO(x) ((x) - KMEM_KBASE)
#define P2V_WO(x) ((x) + KMEM_KBASE)

/* In pages - 8MB */
#define KMEM_STACK_LIMIT     CONFIG_KERNEL_PROGRAM_STACK

/* The start and end of a program's stack, in virtual memory. The stack goes
 * from KMEM_PROG_STACK_START TO (KMEM_PROG_STACK_END - 1) */
#define KMEM_PROG_STACK_END (void *)KMEM_KBASE
#define KMEM_PROG_STACK_START (void *)(KMEM_PROG_STACK_END - KMEM_STACK_LIMIT * PG_SIZE)

/* Location that a program's data will be loaded into */
#define KMEM_PROG_LINK (void *)CONFIG_KERNEL_PROGRAM_LINK

#endif
