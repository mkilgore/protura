/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/mm/palloc.h>
#include <protura/bits.h>

#include <arch/paging.h>
#include <arch/pages.h>

void arch_pages_init(pa_t first_addr, pa_t last_addr)
{
    pa_t first_page = PG_ALIGN(first_addr);

    for (; first_page < last_addr; first_page += PG_SIZE) {
        struct page *p = page_from_pa(first_page);
        bit_clear(&p->flags, PG_INVALID);

        if (first_page >= V2P(CONFIG_KERNEL_KMAP_START)) {
            kp(KP_WARNING, "High memory not supported, memory past %p is not usable\n", (void *)first_page);
            break;
        }

        __mark_page_free(first_page);
    }
}

