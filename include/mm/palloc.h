/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_MM_PALLOC_H
#define INCLUDE_MM_PALLOC_H

#include <protura/types.h>
#include <protura/compiler.h>
#include <protura/atomic.h>
#include <protura/list.h>
#include <arch/align.h>
#include <arch/memlayout.h>
#include <arch/paging.h>

struct inode;

struct page {
    atomic_t use_count;
    pn_t page_number;

    /* The 'order' of the page tells us how many pages were allocated together
     * with this one. Increments are powers of two, so an 'order' of 3 means
     * that this page was allocated together with 2^3 - 1 other pages */
    int order;
    struct list_node page_list_node;

    unsigned int flags;

    struct inode *inode_owner;
    uint32_t inode_offset;
} __align_cacheline;

enum page_flag {
    PG_INVALID = 31,
};

enum {
    __PAL_NOWAIT = 1,
};

#define PAL_ATOMIC (__PAL_NOWAIT)
#define PAL_KERNEL (0)

/*
 * The 'phys' versions of palloc return the physical address of the allocated page.
 * This is useful for the situations where the coresponding page may not have a
 * mapping in kernel space.
 *
 * When palloc returns multiple pages, they are always aligned to the order
 * size. So, if you do palloc_multiple(3, 0), you'll recieve a block of 2^3
 * pages which is aligned to 2^3.
 */
pa_t palloc_phys_multiple(int order, unsigned int flags);

static inline pa_t palloc_phys(unsigned int flags)
{
    return palloc_phys_multiple(0, flags);
}


void *palloc_multiple(int order, unsigned int flags)
{
    return P2V(palloc_phys_multiple(order, flags));
}

static inline void *palloc(unsigned int flags)
{
    return palloc_multiple(0, flags);
}

void pfree_phys_multiple(pa_t, int order);

static inline void pfree_phys(pa_t pa)
{
    pfree_phys_multiple(pa, 0);
}

static inline void pfree_multiple(va_t addr, int order)
{
    pfree_phys_multiple(V2P(addr), order);
}

static inline void pfree(va_t addr)
{
    pfree_multiple(addr, 0);
}

struct page *page_get_from_pn(pn_t);

static inline struct page *page_get_from_pa(pa_t pa)
{
    return page_get_from_pn(__PN(pa));
}

static inline struct page *page_get(void *va)
{
    return page_get_from_pn(__PN(V2P(va)));
}

static inline pa_t page_to_pa(struct page *p)
{
    return (p->page_number) << PG_SHIFT;
}

void palloc_init(void **kbrk, int pages);

#endif
