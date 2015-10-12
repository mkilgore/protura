/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/bits.h>
#include <protura/list.h>
#include <protura/spinlock.h>
#include <protura/scheduler.h>
#include <protura/wait.h>

#include <mm/palloc.h>

struct page_buddy_map {
    struct list_head free_pages;
    int free_count;
    struct wait_queue wait_for_free;
};

struct page_buddy_alloc {
    spinlock_t lock;

    struct page *pages;
    int page_count;

    struct page_buddy_map *maps;
    int map_count;

    int free_pages;
};

#define PALLOC_MAPS 6

static struct page_buddy_map buddy_maps[PALLOC_MAPS];

static struct page_buddy_alloc buddy_allocator = {
    .lock = SPINLOCK_INIT("Buddy-allocator"),
    .pages = NULL,
    .page_count = 0,

    .maps = buddy_maps,
    .map_count = PALLOC_MAPS,

    .free_pages = 0,
};

struct page *page_get_from_pn(pn_t page_num)
{
    return buddy_allocator.pages + page_num;
}

/* We swap the bit in the 'order' position to get this pages buddy.
 * This works because the buddy is always '2 ^ order' pages away. */
static inline pn_t get_buddy_pn(pn_t pn, int order)
{
    return pn ^ (1 << order);
}

void __pfree_add_pages(struct page_buddy_alloc *alloc, pa_t pa, int order)
{
    int original_order = order, i;
    pn_t cur_page = __PN(pa);
    struct page *p, *buddy;

    while (order < PALLOC_MAPS - 1) {
        buddy = page_get_from_pn(get_buddy_pn(cur_page, order));

        if (buddy->order != order || test_bit(&buddy->flags, PG_INVALID))
            break;

        /* Remove our buddy from it's current free list, then use the lower
         * of our two pages as our new higher-order page, and clear the
         * 'order' value of the other page */
        list_del(&buddy->page_list_node);
        alloc->maps[order].free_count--;

        cur_page &= ~(1 << order);

        p = page_get_from_pn(get_buddy_pn(cur_page, order));
        p->order = -1;

        order++;
    }

    p = page_get_from_pn(cur_page);
    p->order = order;
    list_add(&alloc->maps[order].free_pages, &p->page_list_node);
    alloc->maps[order].free_count++;

    alloc->free_pages += 1 << original_order;

    /* Wake-up any task waiting for a page of this size or larger */
    for (i = 0; i <= original_order; i++)
        wait_queue_wake_all(&alloc->maps[i].wait_for_free);
}

void pfree_phys_multiple(pa_t pa, int order)
{
    using_spinlock(&buddy_allocator.lock)
        __pfree_add_pages(&buddy_allocator, pa, order);
}

/* Breaks apart a page of 'order' size, into to two pages of 'order - 1' size */
static void break_page(struct page_buddy_alloc *alloc, int order, unsigned int flags)
{
    struct page *p, *buddy;

    if (alloc->maps[order].free_count == 0) {
        if (order + 1 < alloc->map_count)
            break_page(alloc, order + 1, flags);
        else
            return ;

        /* It's possible 'break_page()' failed */
        if (alloc->maps[order].free_count == 0)
            return ;
    }

    p = list_take_last(&alloc->maps[order].free_pages, struct page, page_list_node);
    alloc->maps[order].free_count--;

    order--;

    buddy = page_get_from_pn(get_buddy_pn(p->page_number, order));

    p->order = order;
    buddy->order = order;

    list_add(&alloc->maps[order].free_pages, &p->page_list_node);
    list_add(&alloc->maps[order].free_pages, &buddy->page_list_node);
    alloc->maps[order].free_count += 2;
}

static void __palloc_sleep_for_enough_pages(struct page_buddy_alloc *alloc, int order, unsigned int flags)
{
    kprintf("Total free pages: %d\n", alloc->free_pages);
    kprintf("Need: %d\n", 1 << order);
  sleep_again:
    sleep_with_wait_queue(&alloc->maps[order].wait_for_free) {
        if (alloc->free_pages < (1 << order)) {
            not_using_spinlock(&alloc->lock)
                scheduler_task_yield();

            goto sleep_again;
        }
    }
}

static struct page *__palloc_phys_multiple(struct page_buddy_alloc *alloc, int order, unsigned int flags)
{
    if (alloc->maps[order].free_count == 0) {
        break_page(alloc, order + 1, flags);
        if (alloc->maps[order].free_count == 0)
            return 0;
    }

    struct page *p = list_take_last(&alloc->maps[order].free_pages, struct page, page_list_node);
    alloc->maps[order].free_count--;

    p->order = -1;

    return p;
}

pa_t palloc_phys_multiple(int order, unsigned int flags)
{
    struct page *p;
    pa_t ret;

    kprintf("Getting %d pages\n", 1 << order);
    kprintf("Flags: %d\n", flags);

    using_spinlock(&buddy_allocator.lock) {
        if (!(flags & __PAL_NOWAIT))
            __palloc_sleep_for_enough_pages(&buddy_allocator, order, flags);
        p = __palloc_phys_multiple(&buddy_allocator, order, flags);
    }

    if (p)
        ret = page_to_pa(p);
    else
        ret = 0;

    return ret;
}

void palloc_init(void **kbrk, int pages)
{
    struct page *p;
    int i;

    kprintf("Initalizing buddy allocator\n");

    *kbrk = ALIGN_2(*kbrk, alignof(struct page));

    buddy_allocator.page_count = pages;
    buddy_allocator.pages = *kbrk;

    kprintf("Pages: %d, array: %p\n", pages, *kbrk);

    *kbrk += pages * sizeof(struct page);

    memset(buddy_allocator.pages, 0, pages * sizeof(struct page));

    /* All pages start as INVALID. As the arch init code goes, it will call
     * 'free' on any pages which are valid to use. */
    p = buddy_allocator.pages + pages;
    while (p-- >= buddy_allocator.pages) {
        p->order = -1;
        p->page_number = (int)(p - buddy_allocator.pages);
        set_bit(&p->flags, PG_INVALID);
    }

    /* We keep using 'sizeof(*buddy_allocator.maps[0].bitmap)' because we don't
     * want this code dependant on the number of bytes used for the entries in the map.
     *
     * IE. 'bitmap' could be an 'int *', or a 'char *', or a 'uint64_t *'
     * pointer, and this code works with any of them. */
    for (i = 0; i < PALLOC_MAPS; i++) {
        INIT_LIST_HEAD(&buddy_allocator.maps[i].free_pages);
        wait_queue_init(&buddy_allocator.maps[i].wait_for_free);
        buddy_allocator.maps[i].free_count = 0;
    }
}

