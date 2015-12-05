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
#include <arch/task.h>

void arch_address_space_init(struct address_space *addrspc)
{
    addrspc->page_dir = palloc_va(0, PAL_KERNEL);
    memcpy(addrspc->page_dir, &kernel_dir, PG_SIZE);
}

void arch_address_space_clear(struct address_space *addrspc)
{
    struct vm_map *map;
    int i;

    kp(KP_TRACE, "Beginning address_space_clear\n");
    list_foreach_take_entry(&addrspc->vm_maps, map, address_space_entry) {
        kp(KP_TRACE, "Freeing map %p\n", map);
        pfree_unordered(&map->page_list);

        kp(KP_TRACE, "Unmapping %p\n", map);
        address_space_unmap_vm_map(addrspc, map);

        kp(KP_TRACE, "Freeing map %p\n", map);
        kfree(map);
    }

    pgd_t *dir = addrspc->page_dir;

    kp(KP_TRACE, "Kmem_kpage: %d\n", KMEM_KPAGE);

    for (i = 0; i < KMEM_KPAGE; i++) {
        if (dir->entries[i].present) {
            kp(KP_TRACE, "Freeing page at entry %d\n", i);
            pfree_pa(PAGING_FRAME(dir->entries[i].entry), 0);
        }
    }

    pfree_va(addrspc->page_dir, 0);
}

void arch_address_space_copy(struct address_space *new, struct address_space *old)
{
    struct vm_map *map;
    struct vm_map *new_map;

    /* Make a copy of every map */
    list_foreach_entry(&old->vm_maps, map, address_space_entry) {
        struct page *oldp, *newp;

        new_map = kmalloc(sizeof(*map), PAL_KERNEL);
        vm_map_init(new_map);

        new_map->addr = map->addr;
        new_map->flags = map->flags;

        /* Make a copy of each page in this map */
        list_foreach_entry(&map->page_list, oldp, page_list_node) {
            newp = palloc(0, PAL_KERNEL);

            memcpy(newp->virt, oldp->virt, PG_SIZE);

            list_add_tail(&new_map->page_list, &newp->page_list_node);
        }

        address_space_vm_map_add(new, new_map);
    }
}

void arch_address_space_map_vm_entry(struct address_space *addrspc, va_t virtual, pa_t physical, flags_t vm_flags)
{
    int dir_index, table_index;
    pgd_t *dir = addrspc->page_dir;
    pgt_t *table;
    uintptr_t table_entry;

    table_entry = physical | PTE_PRESENT | PTE_USER;

    if (flag_test(&vm_flags, VM_MAP_WRITE))
        table_entry |= PTE_WRITABLE;

    dir_index = PAGING_DIR_INDEX(virtual);
    table_index = PAGING_TABLE_INDEX(virtual);

    if (dir->entries[dir_index].addr == 0) {
        pa_t page = palloc_pa(0, PAL_KERNEL);
        uintptr_t entry = page | PDE_PRESENT | PDE_WRITABLE | PDE_USER;

        dir->entries[dir_index].entry = entry;
    }

    table = P2V(PAGING_FRAME(dir->entries[dir_index].entry));

    table->entries[table_index].entry = table_entry;
}

void arch_address_space_unmap_vm_entry(struct address_space *addrspc, va_t addr)
{
    int dir_index, table_index;
    pgd_t *dir = addrspc->page_dir;
    pgt_t *table;

    addr = PG_ALIGN_DOWN(addr);

    dir_index = PAGING_DIR_INDEX(addr);
    table_index = PAGING_TABLE_INDEX(addr);

    if (!dir->entries[dir_index].present)
        return ;

    table = P2V(PAGING_FRAME(dir->entries[dir_index].entry));

    table->entries[table_index].entry &= ~PTE_PRESENT;

    flush_tlb_single(addr);
}

void arch_address_space_map_vm_map(struct address_space *addrspc, struct vm_map *map)
{
    struct page *p;
    va_t start, end;

    start = PG_ALIGN_DOWN(map->addr.start);
    end = PG_ALIGN(map->addr.end);

    list_foreach_entry(&map->page_list, p, page_list_node) {
        arch_address_space_map_vm_entry(addrspc, start, __PN_TO_PA(p->page_number) ,map->flags);
        start += PG_SIZE;
        if (start >= end)
            break;
    }
}

void arch_address_space_unmap_vm_map(struct address_space *addrspc, struct vm_map *map)
{
    int pages, i;
    va_t start, end;

    start = PG_ALIGN_DOWN(map->addr.start);
    end = PG_ALIGN_DOWN(map->addr.end);

    pages = (end - start) / PG_SIZE;

    for (i = 0; i < pages; i++) {
        arch_address_space_unmap_vm_entry(addrspc, start);
        start += PG_SIZE;
    }
}
