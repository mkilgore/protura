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
#include <protura/snprintf.h>
#include <protura/task.h>
#include <protura/mm/palloc.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/memlayout.h>
#include <protura/mm/vm.h>
#include <protura/fs/vfs.h>

static int mmap_private_fill_page(struct vm_map *map, va_t address)
{
    struct page *p = pzalloc(0, PAL_KERNEL);
    if (!p)
        return -ENOSPC;

    page_table_map_entry(map->owner->page_dir, address, page_to_pa(p), map->flags);
    return 0;
}

int address_space_handle_pagefault(struct address_space *addrspc, va_t address)
{
    struct vm_map *map;

    list_foreach_entry(&addrspc->vm_maps, map, address_space_entry) {
        if (address >= map->addr.start && address < map->addr.end) {
            if (map->ops && map->ops->fill_page)
                return (map->ops->fill_page) (map, address);
            else
                return mmap_private_fill_page(map, address);
        }
    }

    kp(KP_TRACE, "addrspc: No handler for fault: %p\n", address);
    return -EFAULT;
}

void address_space_change(struct address_space *new)
{
    struct task *current = cpu_get_local()->current;
    struct address_space *old = current->addrspc;

    current->addrspc = new;
    page_table_change(new->page_dir);

    address_space_clear(old);
    kfree(old);
}

void address_space_clear(struct address_space *addrspc)
{
    struct vm_map *map;

    list_foreach_take_entry(&addrspc->vm_maps, map, address_space_entry) {
        int page_count = (map->addr.end - map->addr.start) / PG_SIZE;

        page_table_free_range(addrspc->page_dir, map->addr.start, page_count);

        if (map->filp)
            vfs_close(map->filp);

        kfree(map);
    }

    page_table_free(addrspc->page_dir);
    addrspc->page_dir = NULL;
}

static struct vm_map *vm_map_copy(struct address_space *new, struct address_space *old, struct vm_map *old_map)
{
    if (flag_test(&old_map->flags, VM_MAP_NOFORK))
        return NULL;

    struct vm_map *new_map = kmalloc(sizeof(*new_map), PAL_KERNEL);
    vm_map_init(new_map);

    new_map->addr = old_map->addr;
    new_map->flags = old_map->flags;

    int pages = (uintptr_t)(old_map->addr.end - old_map->addr.start) / PG_SIZE;

    /* Make a copy of each page in this map */
    page_table_copy_range(new->page_dir, old->page_dir, old_map->addr.start, pages);

    if (old_map->filp) {
        int err = vfs_open(old_map->filp->inode, old_map->filp->flags, &new_map->filp);
        if (err) {
            kfree(new_map);
            return NULL;
        }

        new_map->file_page_offset = old_map->file_page_offset;
    }
    new_map->ops = old_map->ops;

    return new_map;
}

void address_space_copy(struct address_space *new, struct address_space *old)
{
    struct vm_map *map;
    struct vm_map *new_map;

    /* Make a copy of every map */
    list_foreach_entry(&old->vm_maps, map, address_space_entry) {

        new_map = vm_map_copy(new, old, map);

        if (old->code == map)
            new->code = new_map;
        else if (old->data == map)
            new->data = new_map;
        else if (old->stack == map)
            new->stack = new_map;
        else if (old->bss == map)
            new->bss = new_map;

        address_space_vm_map_add(new, new_map);
    }
}

void address_space_vm_map_add(struct address_space *addrspc, struct vm_map *map)
{
    list_add(&addrspc->vm_maps, &map->address_space_entry);
    map->owner = addrspc;
}

void address_space_vm_map_remove(struct address_space *addrspc, struct vm_map *map)
{
    list_del(&map->address_space_entry);
    map->owner = NULL;
}

static void vm_map_resize_start(struct vm_map *map, va_t new_start)
{
    pgd_t *pgd = map->owner->page_dir;

    if (map->addr.start <= new_start) {
        int old_pages = (new_start - map->addr.start) / PG_SIZE;
        va_t new_addr = map->addr.start;

        page_table_free_range(pgd, new_addr, old_pages);
    }

    map->addr.start = new_start;
}

static void vm_map_resize_end(struct vm_map *map, va_t new_end)
{
    pgd_t *pgd = map->owner->page_dir;

    if (new_end <= map->addr.end) {
        int old_pages = (map->addr.end - new_end) / PG_SIZE;

        if (old_pages)
            page_table_free_range(pgd, map->addr.end - old_pages * PG_SIZE, old_pages);
    }

    map->addr.end = new_end;
}

void vm_map_resize(struct vm_map *map, struct vm_region new_size)
{
    if (map->addr.start != new_size.start)
        vm_map_resize_start(map, new_size.start);

    if (map->addr.end != new_size.end)
        vm_map_resize_end(map, new_size.end);
}
