/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/multiboot.h>
#include <protura/string.h>
#include <protura/debug.h>
#include <mm/memlayout.h>

#include <arch/kbrk.h>
#include <arch/paging.h>

struct allocator {
    uintptr_t phys_start;
    uint32_t pages;
    uint8_t bitmap[];
};

/* A zeroed page_directory we can use for our initial table */
__align(0x1000) static struct page_table kernel_dir[1024] = {
    { .entry = 0 }
};

__align(0x1000) static struct page kernel_pgt[1024] = {
    { .entry = 0 }
};

__align(0x1000) static struct page kernel_low_mem_pgt[KMEM_KTABLES][1024] = {
    { { .entry = 0 } }
};

static struct allocator *low_mem_alloc;
static struct allocator *high_mem_alloc;

void page_directory_init(uintptr_t kern_end)
{
    uintptr_t cur_page, last_page = (uintptr_t)V2P(PG_ALIGN(kern_end)) >> 12, cur_table;
    int table;
    kernel_dir[KMEM_KPAGE].entry = (uintptr_t)V2P(kernel_pgt) | PDE_PRESENT | PDE_WRITABLE;

    for (table = 0; table < KMEM_KTABLES; table++) {
        kernel_dir[KMEM_KPAGE + table + 1].entry = (uintptr_t)V2P(kernel_low_mem_pgt[table]) | PDE_PRESENT | PDE_WRITABLE;
        kprintf("Mapped %x to %x\n", KMEM_KPAGE + table + 1, V2P(kernel_low_mem_pgt[table]));
    }

    for (cur_page = 0; cur_page < last_page; cur_page++)
        kernel_pgt[cur_page].entry = cur_page << 12 | PTE_PRESENT | PTE_WRITABLE;

    for (cur_table = 0; cur_table < KMEM_KTABLES; cur_table++)
        for (cur_page = 0 ;cur_page < 1024; cur_page++) {
            kernel_low_mem_pgt[cur_table][cur_page].entry = ((cur_table << 22) + (cur_page << 12)) | PTE_PRESENT | PTE_WRITABLE;
            if (cur_page << 12 == 0x00133000) {
                kprintf("Mapped page %x, cur_page: %d\n", cur_page << 12, cur_page);
            }
        }

    set_current_page_directory((uintptr_t)V2P(kernel_dir));
}

void paging_init(uintptr_t kernel_end, uintptr_t last_physical_address)
{
    uintptr_t k_end, k_start;
    size_t low_size, high_size;

    low_size = ((KMEM_LOW_SIZE >> 12) >> 3) + 1;
    high_size = (((last_physical_address - V2P(kernel_end)) >> 12) >> 3) + 1;

    low_mem_alloc = kbrk(sizeof(struct allocator) + low_size, alignof(struct allocator));
    high_mem_alloc = kbrk(sizeof(struct allocator) + high_size, alignof(struct allocator));

    get_kernel_addrs(&k_start, &k_end);

    k_start = PG_ALIGN(k_start);
    k_end = PG_ALIGN(k_end);

    low_mem_alloc->pages = KMEM_LOW_SIZE >> 12;
    high_mem_alloc->pages = (last_physical_address - V2P(k_end) - KMEM_LOW_SIZE) >> 12;

    low_mem_alloc->phys_start = V2P(k_end);
    high_mem_alloc->phys_start = V2P(k_end) + KMEM_LOW_SIZE;

    memset(low_mem_alloc->bitmap, 0, low_size);
    memset(high_mem_alloc->bitmap, 0, high_size);

    page_directory_init(k_end);

    return ;
}

static uintptr_t get_page(struct allocator *alloc)
{
    uint8_t mask;
    uint32_t cur_byte, max = alloc->pages >> 3;
    uint32_t page_n = 0;

    for (cur_byte = 0; cur_byte < max; cur_byte++) {
        for (mask = 0; mask != 8; mask++) {
            if ((alloc->bitmap[cur_byte] & (1 << mask)) == 0) {
                page_n = (cur_byte << 3) + mask;
                alloc->bitmap[cur_byte] |= (1 << mask);

                goto eloop;
            }
        }
    }

eloop:

    return alloc->phys_start + (page_n << 12);
}

static void free_page(struct allocator *alloc, uintptr_t p)
{
    uint8_t mask;
    uint32_t byte;
    uintptr_t offset;

    if (alloc->phys_start > p || (alloc->phys_start + (alloc->pages << 12)) < p)
        return;

    offset = p - alloc->phys_start;
    byte = offset >> 15;
    mask = (offset >> 12) & 0x7;

    alloc->bitmap[byte] &= ~(1 << mask);
}

uintptr_t low_get_page(void)
{
    return get_page(low_mem_alloc);
}

void low_free_page(uintptr_t p)
{
    free_page(low_mem_alloc, p);
}

uintptr_t high_get_page(void)
{
    return get_page(high_mem_alloc);
}

void high_free_page(uintptr_t p)
{
    free_page(high_mem_alloc, p);
}

