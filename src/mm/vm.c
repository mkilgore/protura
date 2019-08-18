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

        kfree(map);
    }

    page_table_free(addrspc->page_dir);
    addrspc->page_dir = NULL;
}

static struct vm_map *vm_map_copy(struct address_space *new, struct address_space *old, struct vm_map *old_map)
{
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
    kp(KP_TRACE, "vm_map: %p: start: %p, end: %p, resize start: %p\n", map, map->addr.start, map->addr.end, new_start);

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
    kp(KP_TRACE, "vm_map: %p: start: %p, end: %p, resize end: %p\n", map, map->addr.start, map->addr.end, new_end);

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

int user_check_region(const void *ptr, size_t size, flags_t vm_flags)
{
    struct task *t = cpu_get_local()->current;
    struct address_space *addrspc = t->addrspc;
    struct vm_map *cur;

    if (!user_ptr_check_is_on())
        return 0;

    list_foreach_entry(&addrspc->vm_maps, cur, address_space_entry) {
        if (cur->addr.start <= ptr && cur->addr.end >= ptr + size) {
            if ((cur->flags & vm_flags) == vm_flags)
                return 0;
            else
                return -EFAULT;
        }
    }

    return -EFAULT;
}

int user_check_strn(const void *ptr, size_t size, flags_t vm_flags)
{
    struct task *t = cpu_get_local()->current;
    struct address_space *addrspc = t->addrspc;
    struct vm_map *cur;

    if (!user_ptr_check_is_on())
        return 0;

    list_foreach_entry(&addrspc->vm_maps, cur, address_space_entry) {
        if (cur->addr.start <= ptr && cur->addr.end > ptr) {
            va_t end = (va_t)ptr + size;
            if (end >= cur->addr.end)
                end = cur->addr.end - 1;

            if ((cur->flags & vm_flags) != vm_flags)
                return -EFAULT;

            const char *c;
            for (c = ptr; c <= (const char *)end; c++)
                if (*c == '\0')
                    return 0;

            return -EFAULT;
        }
    }

    return -EFAULT;
}

static va_t get_new_bss_start(struct address_space *addrspc)
{
    if (addrspc->data)
        return addrspc->data->addr.end;
    else
        return addrspc->code->addr.end;
}

static struct vm_map *create_bss(struct address_space *addrspc)
{
    va_t bss_start = get_new_bss_start(addrspc);
    struct vm_map *bss;

    bss = kmalloc(sizeof(*bss), PAL_KERNEL);
    vm_map_init(bss);

    bss->addr.start = bss_start;
    bss->addr.end = bss_start + PG_SIZE;

    flag_set(&bss->flags, VM_MAP_WRITE);
    flag_set(&bss->flags, VM_MAP_READ);

    address_space_vm_map_add(addrspc, bss);

    addrspc->bss = bss;
    addrspc->brk = bss->addr.start;

    return bss;
}

void *sys_sbrk(intptr_t increment)
{
    struct vm_map *bss;
    va_t old;
    struct task *t = cpu_get_local()->current;
    struct address_space *addrspc = t->addrspc;

    kp(KP_TRACE, "sbrk: %d\n", increment);

    bss = addrspc->bss;

    if (!bss)
        bss = create_bss(addrspc);

    old = addrspc->brk;

    if (increment == 0)
        return old;

    if (bss->addr.end < PG_ALIGN(old + increment))
        vm_map_resize(bss, (struct vm_region) { .start = bss->addr.start, .end = PG_ALIGN(old + increment) });

    addrspc->brk = old + increment;

    return old;
}

void sys_brk(va_t new_end)
{
    struct vm_map *bss;
    struct task *t = cpu_get_local()->current;
    va_t new_end_aligned;

    new_end_aligned = PG_ALIGN(new_end);
    bss = t->addrspc->bss;

    t->addrspc->brk = new_end;

    /* Check if we have a bss segment, and create a new one after the end of
     * the code segment if we don't */
    if (!bss)
        bss = create_bss(t->addrspc);

    /* Expand or shrink the current bss segment */
    if (bss->addr.start >= new_end && bss->addr.end < new_end_aligned)
        vm_map_resize(bss, (struct vm_region) { .start = bss->addr.start, .end = new_end_aligned });
    else if (bss->addr.start > new_end) /* Can happen since the "bss" can start at the end of the data segment */
        vm_map_resize(bss, (struct vm_region) { .start = bss->addr.start, .end = bss->addr.start + PG_SIZE });
}
