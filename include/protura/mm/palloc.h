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
#include <protura/debug.h>
#include <protura/compiler.h>
#include <protura/atomic.h>
#include <protura/list.h>
#include <protura/string.h>
#include <protura/bits.h>
#include <protura/mm/ptable.h>
#include <arch/align.h>
#include <arch/memlayout.h>
#include <arch/log2.h>

struct inode;

struct page {
    atomic_t use_count;
    pn_t page_number;

    /* The 'order' of the page tells us how many pages were allocated together
     * with this one. Increments are powers of two, so an 'order' of 3 means
     * that this page was allocated together with 2^3 - 1 other pages */
    int order;
    list_node_t page_list_node;

    /* The starting and length of the data contained - used for caches and
     * such. Has no set use and can be used for anything by the owner of this
     * page */
    size_t startb, lenb;

    flags_t flags;

    void *virt;
} __align_cacheline;

enum page_flag {
    PG_INVALID = 31,
};

enum {
    __PAL_NOWAIT = 1,
};

#define PAL_ATOMIC (__PAL_NOWAIT)
#define PAL_KERNEL (0)

/* Essencially a faster version of pfree -- Note no locking is used, only
 * useful for boot-up when marking pages as free. */
void __mark_page_free(pa_t pa);

struct page *page_from_pn(pn_t);

#define page_from_pa(pa) page_from_pn(__PA_TO_PN(pa))
#define page_from_va(va) page_from_pa(V2P(va))

#define page_to_pa(page) __PN_TO_PA((page)->page_number)


/* Used to allocate physically-contiguous pages in physical memory. Since not
 * all physical memory is mapped at a virual address, the standard way to
 * access a page is via it's coresponding `struct page` entry.
 * */
struct page *palloc(int order, unsigned int flags);

/* Useful for allocating kernel memory, which will always have a coresponding
 * virtual address. Returns the address of the first page if you allocate
 * multiple pages. */
static inline void *palloc_va(int order, unsigned int flags)
{
    struct page *p = palloc(order, flags);
    if (p)
        return p->virt;
    else
        return NULL;
}

/* Returns the first page's coresponding physical address */
static inline pa_t palloc_pa(int order, unsigned int flags)
{
    struct page *p = palloc(order, flags);
    if (p)
        return __PN_TO_PA(p->page_number);
    else
        return 0;
}

static inline struct page *pzalloc(int order, unsigned int flags)
{
    struct page *page = palloc(order, flags);
    if (page)
        memset(page->virt, 0, PG_SIZE);

    return page;
}

static inline void *pzalloc_va(int order, unsigned int flags)
{
    struct page *p = pzalloc(order, flags);
    if (p)
        return p->virt;
    else
        return NULL;
}

static inline pa_t pzalloc_pa(int order, unsigned int flags)
{
    struct page *p = pzalloc(order, flags);
    if (p)
        return __PN_TO_PA(p->page_number);
    else
        return 0;
}


/* Used for freeing addresses returned from the above palloc calls. Note that
 * you have to provide the order. */
void pfree(struct page *, int order);

static inline void pfree_va(va_t va, int order)
{
    pfree(page_from_va(va), order);
}

static inline void pfree_pa(pa_t pa, int order)
{
    pfree(page_from_pa(pa), order);
}

/* `void *` to make it compatible with both `void *` and `char *` */
static inline void pfree_va_cleanup(void *p)
{
    char **page = p;
    if (*page)
        pfree_va(*page, 0);
}

#define __pfree_single_va \
    __cleanup(pfree_va_cleanup)

/* Used to allocate 'count' pages from memory. The returned pages do *not* have
 * contiguous physical addresses. As a consiquence, the pages are 'returned' by
 * attaching them to the provided list_head_t.
 *
 * pfree_unordered free's all the pages attached to the provided list_head_t,
 * and removes them from the list. */
int palloc_unordered(list_head_t *head, int count, unsigned int flags);
void pfree_unordered(list_head_t *head);

void palloc_init(void **kbrk, int pages);

int palloc_free_page_count(void);

#endif
