/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <mm/palloc.h>
#include <protura/bits.h>

#include <arch/paging.h>
#include <arch/pages.h>

void arch_pages_init(pa_t kernel_start, pa_t kernel_end, pa_t last_physical_addr)
{
    pa_t first_page = PG_ALIGN(kernel_end);

    for (; first_page < last_physical_addr; first_page += PG_SIZE) {
        struct page *p = page_get_from_pa(first_page);
        bit_clear(&p->flags, PG_INVALID);

        pfree_phys(first_page);
    }
}

