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
#include <protura/list.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/memlayout.h>
#include <protura/mm/vm.h>
#include <protura/scheduler.h>
#include <protura/task.h>
#include <protura/mm/palloc.h>

#include <arch/paging.h>
#include <arch/ptable.h>

pgd_t *page_table_new(void)
{
    pgd_t *pgd = palloc_va(0, PAL_KERNEL);
    memcpy(pgd, &kernel_dir, PG_SIZE);
    return pgd;
}

void page_table_free(pgd_t *table)
{
    pde_t *pde;
    pa_t pa;

    pgd_foreach_pde(table, pde) {
        if (!pde_exists(pde))
            continue;

        if (!pde_is_user(pde))
            continue;

        pa = pde_get_pa(pde);
        if (pa)
            pfree_pa(pa, 0);
    }

    pfree_va(table, 0);
}

void page_table_change(pgd_t *new)
{
    set_current_page_directory(V2P(new));
}
