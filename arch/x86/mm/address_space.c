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

void page_table_map_entry(pgd_t *dir, va_t virtual, pa_t physical, flags_t vm_flags)
{
    pte_t table_entry = mk_pte(physical, PTE_PRESENT | PTE_USER);

    if (flag_test(&vm_flags, VM_MAP_WRITE))
        pte_set_writable(&table_entry);

    pde_t *pde = pgd_get_pde(dir, virtual);

    if (!pde_exists(pde)) {
        pa_t page = pzalloc_pa(0, PAL_KERNEL);

        *pde = mk_pde(page, PDE_PRESENT | PDE_WRITABLE | PDE_USER);
    }

    pgt_t *pgt = pde_to_pgt(pde);

    pte_t *pte = pgt_get_pte(pgt, virtual);
    *pte = table_entry;
}

void page_table_unmap_entry(pgd_t *dir, va_t addr)
{
    pde_t *pde;
    pgt_t *pgt;
    pte_t *pte;

    pde = pgd_get_pde(dir, addr);

    if (!pde_exists(pde))
        return ;

    pgt = pde_to_pgt(pde);
    pte = pgt_get_pte(pgt, addr);

    pte_clear_pa(pte);
    flush_tlb_single(addr);
}

pte_t *page_table_get_entry(pgd_t *dir, va_t addr)
{
    pde_t *pde;
    pgt_t *pgt;

    pde = pgd_get_pde(dir, addr);

    if (!pde_exists(pde))
        return NULL;

    pgt = pde_to_pgt(pde);
    return pgt_get_pte(pgt, addr);
}

void page_table_copy_range(pgd_t *new, pgd_t *old, va_t virtual, int pages)
{
    int dir = pgd_offset(virtual);
    int pg = pgt_offset(virtual);

    int dir_end = pgd_offset(virtual + pages * PG_SIZE);;
    int pg_end = pgt_offset(virtual + pages * PG_SIZE);;

    for (; dir <= dir_end; dir++) {
        pde_t *pde_old = pgd_get_pde_offset(old, dir);
        if (!pde_exists(pde_old))
            continue;

        pgt_t *pgt_old = pde_to_pgt(pde_old);
        int end = PGT_INDEXES;

        if (dir == dir_end)
            end = pg_end;

        pgt_t *pgt_new;

        if (pg < end) {
            pde_t *pde_new = pgd_get_pde_offset(new, dir);
            if (!pde_exists(pde_new)) {
                pa_t pde_page = pzalloc_pa(0, PAL_KERNEL);

                pde_set_pa(pde_new, pde_page);
                pde_set_writable(pde_new);
                pde_set_user(pde_new);
            }
            pgt_new = pde_to_pgt(pde_new);
        }

        for (; pg < end; pg++) {
            pte_t *pte_old = pgt_get_pte_offset(pgt_old, pg);
            if (!pte_exists(pte_old))
                continue;

            void *page = P2V(pte_get_pa(pte_old));
            void *new_page = palloc_va(0, PAL_KERNEL);

            memcpy(new_page, page, PG_SIZE);

            pte_t *pte_new = pgt_get_pte_offset(pgt_new, pg);
            pte_set_pa(pte_new, V2P(new_page));
            pte_set_user(pte_new);

            if (pte_writable(pte_old))
                pte_set_writable(pte_new);
            else
                pte_unset_writable(pte_new);
        }
    }
}

void page_table_free_range(pgd_t *table, va_t virtual, int pages)
{
    int dir = pgd_offset(virtual);
    int pg = pgt_offset(virtual);

    int dir_end = pgd_offset(virtual + pages * PG_SIZE);;
    int pg_end = pgt_offset(virtual + pages * PG_SIZE);;

    for (; dir <= dir_end; dir++) {
        pde_t *pde = pgd_get_pde_offset(table, dir);
        if (!pde_exists(pde))
            continue;

        pgt_t *pgt = pde_to_pgt(pde);

        int end = PGT_INDEXES;

        if (dir == dir_end)
            end = pg_end;

        for (; pg < end; pg++) {
            pte_t *pte = pgt_get_pte_offset(pgt, pg);
            if (!pte_exists(pte))
                continue;

            pa_t page = pte_get_pa(pte);
            if (page)
                pfree_pa(page, 0);

            pte_clear_pa(pte);
        }
    }
}

int pgd_ptr_is_valid(pgd_t *pgd, va_t va)
{
    pde_t *pde = pgd_get_pde(pgd, va);

    if (!pde || !pde_exists(pde))
        return 0;

    if (pde_is_huge(pde))
        return 1;

    pgt_t *pgt = pde_to_pgt(pde);
    pte_t *pte = pgt_get_pte(pgt, va);

    return pte && pte_exists(pte);
}

void page_table_change(pgd_t *new)
{
    set_current_page_directory(V2P(new));
}
