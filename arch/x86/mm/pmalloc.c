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
#include <protura/spinlock.h>
#include <mm/memlayout.h>
#include <mm/bitmap_alloc.h>

#include <arch/asm.h>
#include <arch/alloc.h>
#include <arch/paging.h>
#include <arch/bootmem.h>
#include <arch/pmalloc.h>


static struct bitmap_alloc pmem_alloc;
static struct spinlock pmem_lock = SPINLOCK_INIT("pmalloc-lock");

void pmalloc_init(va_t kernel_end, pa_t last_physical_address)
{
    size_t memory_byte_size;

    memory_byte_size = last_physical_address;
    if (memory_byte_size > 0x40000000)
        memory_byte_size = 0x40000000;

    memory_byte_size -= v_to_p(kernel_end);

    kprintf("pmalloc: memory byte count: %d\n", memory_byte_size);

    pmem_alloc.addr_start = PG_ALIGN(V2P(kernel_end));

    kprintf("pmalloc: kernel memory page start: 0x%x\n", pmem_alloc.addr_start);

    pmem_alloc.page_count = memory_byte_size >> 12;
    pmem_alloc.page_size = PG_SIZE;

    kprintf("pmalloc: kernel page count: %d\n", pmem_alloc.page_count);

    kprintf("pmalloc: bitmap pages: 0x%x\n", pmem_alloc.page_count / 8 + 1);

    pmem_alloc.bitmap_size = pmem_alloc.page_count / 8 + 1;
    pmem_alloc.bitmap = P2V(bootmem_alloc_pages(pmem_alloc.bitmap_size / PG_SIZE + 1));
    memset(pmem_alloc.bitmap, 0, pmem_alloc.bitmap_size);
}

void pmalloc_page_set(pa_t page)
{
    int page_n = (page - pmem_alloc.addr_start) >> 12;

    using_spinlock(&pmem_lock)
        pmem_alloc.bitmap[page_n / 8] |= (1 << (page_n % 8));
}

pa_t pmalloc_page_alloc(int flags)
{
    pa_t page;

    using_spinlock(&pmem_lock)
        page = bitmap_alloc_get_page(&pmem_alloc);

    return page;
}

pa_t pmalloc_pages_alloc(int flags, size_t count)
{
    pa_t page;

    using_spinlock(&pmem_lock)
        page = bitmap_alloc_get_pages(&pmem_alloc, count);

    return page;
}

void pmalloc_page_free(pa_t page)
{
    using_spinlock(&pmem_lock)
        bitmap_alloc_free_page(&pmem_alloc, page);
}

void pmalloc_pages_free(pa_t page, size_t count)
{
    using_spinlock(&pmem_lock)
        bitmap_alloc_free_pages(&pmem_alloc, page, count);
}

