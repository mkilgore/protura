/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/string.h>
#include <protura/debug.h>
#include <protura/atomic.h>
#include <mm/memlayout.h>
#include <mm/bitmap_alloc.h>

#include <arch/asm.h>
#include <arch/idt.h>
#include <arch/alloc.h>
#include <arch/paging.h>
#include <arch/pmalloc.h>
#include <arch/bootmem.h>

static atomic32_t bootmem_done = ATOMIC32_INIT(0);

static struct bitmap_alloc bootmem_allocator;
static uint8_t bootmem_bitmap[((KMEM_KERNEL_SIZE)/ PG_SIZE) / 8];

void bootmem_init(pa_t kernel_end)
{
    pa_t kernel_map_end;
    /* Calculate the number of pages left in the kernel's initial page table
     * We can do this easy because every page-table holds 16MB worth of pages.
     * So we can just align the kernel_end location with 16MB to get the next
     * 16MB mark from the end. Then subtract the two and we have the number of
     * bytes we can make use of after the kernel.
     *
     * Then it's just a matter of calculating the number of pages in that
     * space. */
    kernel_map_end = KMEM_KERNEL_SIZE;

    bootmem_allocator.addr_start = PG_ALIGN(kernel_end);

    bootmem_allocator.page_size = PG_SIZE;
    bootmem_allocator.page_count = (PG_ALIGN(kernel_map_end - kernel_end) >> 12) - 1;

    bootmem_allocator.bitmap = bootmem_bitmap;
    bootmem_allocator.bitmap_size = ARRAY_SIZE(bootmem_bitmap);

    kprintf("bootmem: physical start: %p\n", (void *)bootmem_allocator.addr_start);
    kprintf("bootmem: pages: %d\n", bootmem_allocator.page_count);
    kprintf("bootmem: page size: %d\n", bootmem_allocator.page_size);
}

void bootmem_free_page(pa_t page_addr)
{
    if (atomic32_get(&bootmem_done))
        panic("bootmem: Error! Attempted to use bootmem after pmalloc is up!\n");
    bitmap_alloc_free_page(&bootmem_allocator, page_addr);
}

void bootmem_free_pages(pa_t page_addr, int pages)
{
    if (atomic32_get(&bootmem_done))
        panic("bootmem: Error! Attempted to use bootmem after pmalloc is up!\n");
    bitmap_alloc_free_pages(&bootmem_allocator, page_addr, pages);
}

pa_t bootmem_alloc_page(void)
{
    if (atomic32_get(&bootmem_done))
        panic("bootmem: Error! Attempted to use bootmem after pmalloc is up!\n");

    uintptr_t addr = bitmap_alloc_get_page(&bootmem_allocator);
    if (addr == 0)
        panic("bootmem: Out of memory!!\n");

    return addr;
}

pa_t bootmem_alloc_pages(int pages)
{
    if (atomic32_get(&bootmem_done))
        panic("bootmem: Error! Attempted to use bootmem after pmalloc is up!\n");

    uintptr_t addr = bitmap_alloc_get_pages(&bootmem_allocator, pages);
    if (addr == 0)
        panic("bootmem: Out of memory!!\n");

    return addr;
}

void bootmem_transfer_to_pmalloc(void)
{
    int i;
    kprintf("bootmem: Transfering bootmem to pmalloc\n");
    for (i = 0; i < bootmem_allocator.bitmap_size * 8; i++)
        if (bootmem_allocator.bitmap[i / 8] & (1 << (i % 8)))
            pmalloc_page_set(bootmem_allocator.addr_start + (i * bootmem_allocator.page_size));
    kprintf("bootmem: Transfer done! usage of bootmem will now panic\n");
    atomic32_set(&bootmem_done, 1);
}

