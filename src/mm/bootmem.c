/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/string.h>
#include <protura/snprintf.h>
#include <protura/debug.h>
#include <protura/bits.h>
#include <protura/limits.h>
#include <protura/mm/memlayout.h>
#include <protura/mm/palloc.h>
#include <protura/backtrace.h>
#include <protura/kparam.h>

#include <protura/mm/bootmem.h>

struct bootmem_region {
    pa_t start;
    pa_t end;
};

/* Max number of memory regions that bootmem can handle */
static struct bootmem_region memregions[64];
static pn_t highest_page;

extern char kern_end, kern_start;

static void *kernel_end_address = &kern_end;
static void *kernel_start_address = &kern_start;

int bootmem_add(pa_t start, pa_t end)
{
    int i = 0;

    start = PG_ALIGN(start);
    end = PG_ALIGN_DOWN(end);
    size_t size = end - start;

    /* Skip the zero page, we don't alow allocating PA 0 even if it's valid memory. */
    if (start == 0) {
        start = PG_SIZE;
        if (start >= end)
            return 0;
    }

    /* Keep track of the highest page of physical memory. palloc() needs this
     * to know how big to make the `struct page *` array */
    pn_t last_page = __PA_TO_PN(end);
    if (last_page > highest_page)
        highest_page = last_page;

    pa_t kern_pg_start = PG_ALIGN_DOWN(V2P(kernel_start_address));
    pa_t kern_pg_end = PG_ALIGN(V2P(kernel_end_address));

    /* If the entire region is within the kernel's location, skip it */
    if (start >= kern_pg_start  && end < kern_pg_end)
        return 0;

    /* Check if this region overlaps with the kernel's physical location
     * If so, split into two separate regions and exclude the kernel */
    if ((start < kern_pg_end && end >= kern_pg_end)
        || (start <= kern_pg_start && end > kern_pg_start)) {

        if (start < kern_pg_start)
            bootmem_add(start, kern_pg_start);

        if (end > kern_pg_end)
            bootmem_add(kern_pg_end, end);

        return 0;
    }

    kp(KP_NORMAL, "bootmem: Registering region 0x%08x-0x%08x, %dMB\n", start, end, size / 1024 / 1024);

    for (; i < ARRAY_SIZE(memregions); i++) {
        if (memregions[i].start == 0) {
            memregions[i].start = start;
            memregions[i].end = end;
            return 0 ;
        }
    }

    kp(KP_WARNING, "bootmem: Ran out of regions, discarded region 0x%08x-0x%08x!!!\n", start, end);
    return -ENOMEM;
}

void *bootmem_alloc_nopanic(size_t length, size_t alignment)
{
    struct bootmem_region *region = memregions;

    for (; region != memregions + ARRAY_SIZE(memregions); region++) {
        pa_t first_addr = ALIGN_2(region->start, alignment);
        pa_t end_addr = ALIGN_2_DOWN(region->end, alignment);

        if (first_addr < end_addr && end_addr - first_addr >= length) {
            region->start = first_addr + length;
            return P2V(first_addr);
        }
    }

    return NULL;
}

void *bootmem_alloc(size_t length, size_t alignment)
{
    void *ret = bootmem_alloc_nopanic(length, alignment);

    if (!ret)
        panic("Ran out of bootmem() memory!!!!\n");

    return ret;
}

void bootmem_setup_palloc(void)
{
    palloc_init(highest_page + 1);

    int i = 0;
    for (; i < ARRAY_SIZE(memregions); i++) {
        if (!memregions[i].start)
            continue;

        pa_t first_page = PG_ALIGN(memregions[i].start);
        pa_t last_page = PG_ALIGN_DOWN(memregions[i].end);

        for (; first_page < last_page; first_page += PG_SIZE) {
            struct page *p = page_from_pa(first_page);
            bit_clear(&p->flags, PG_INVALID);

            if (first_page >= V2P(CONFIG_KERNEL_KMAP_START)) {
                kp(KP_WARNING, "High memory not supported, memory past %p is not usable\n", (void *)first_page);
                break;
            }

            __mark_page_free(first_page);
        }
    }
}

#ifdef CONFIG_KERNEL_TESTS
# include "bootmem_test.c"
#endif
