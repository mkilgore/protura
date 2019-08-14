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

    kp(KP_TRACE, "Beginning address_space_clear\n");
    list_foreach_take_entry(&addrspc->vm_maps, map, address_space_entry) {
        kp(KP_TRACE, "Freeing map %p\n", map);
        int page_count = (map->addr.end - map->addr.start) / PG_SIZE;

        kp(KP_TRACE, "Unmapping %p\n", map);
        page_table_free_range(addrspc->page_dir, map->addr.start, page_count);

        kp(KP_TRACE, "Freeing map %p\n", map);
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

    if (map->addr.start > new_start) {
        /* Count the number of new pages needed - Note that because
         * map->addr.start, and new_start are not required to be page aligned,
         * this number may actually be zero if both addresses are in the same
         * page. Thus, we align both addresses to the containing page. */
        int new_pages = (map->addr.start - new_start) / PG_SIZE;
        int i;
        va_t new_addr = map->addr.start - PG_SIZE;

        for (i = 0; i < new_pages; i++, new_addr -= PG_SIZE) {
            struct page *p = palloc(0, PAL_KERNEL);

            page_table_map_entry(pgd, new_addr, page_to_pa(p), map->flags);
        }
    } else {
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

    if (new_end > map->addr.end) {
        int new_pages = (new_end - map->addr.end) / PG_SIZE;
        int i;
        va_t cur_page = map->addr.end;

        for (i = 0; i < new_pages; i++, cur_page += PG_SIZE) {
            kp(KP_TRACE, "vm_map resize: %p, cur_page: %p\n", map, cur_page);
            struct page *p = pzalloc(0, PAL_KERNEL);

            page_table_map_entry(pgd, cur_page, page_to_pa(p), map->flags);
        }
    } else {
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

    kp(KP_TRACE, "Checking user pointer %p(%d)\n", ptr, size);

    list_foreach_entry(&addrspc->vm_maps, cur, address_space_entry) {
        kp(KP_TRACE, "Checking region %p-%p\n", cur->addr.start, cur->addr.end);
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

static void create_data(struct address_space *addrspc, va_t new_end)
{
    struct vm_map *data;
    va_t data_start = PG_ALIGN(addrspc->code->addr.end);
    pgd_t *pgd = addrspc->page_dir;

    if (new_end > data_start) {
        data = kmalloc(sizeof(*data), PAL_KERNEL);
        vm_map_init(data);

        data->addr.start = data_start;
        data->addr.end = new_end;

        flag_set(&data->flags, VM_MAP_WRITE);
        flag_set(&data->flags, VM_MAP_READ);

        int pages = (new_end - data_start) / PG_SIZE;

        int i;
        for (i = 0; i < pages; i++) {
            pa_t page = pzalloc_pa(0, PAL_KERNEL);

            page_table_map_entry(pgd, PG_ALIGN_DOWN(data->addr.start) + pages * PG_SIZE, page, data->flags);
        }

        address_space_vm_map_add(addrspc, data);

        addrspc->data = data;
    }
}

void *sys_sbrk(intptr_t increment)
{
    struct vm_map *data;
    va_t old;
    struct task *t = cpu_get_local()->current;

    kp(KP_TRACE, "%p: sbrk: %d\n", t, increment);

    data = t->addrspc->data;

    if (!data) {
        va_t data_start = t->addrspc->code->addr.end;

        create_data(t->addrspc, PG_ALIGN(data_start + increment));

        if (t->addrspc->data)
            return data_start;
        else
            return NULL;
    }

    old = data->addr.end;

    vm_map_resize(data, (struct vm_region) { .start = data->addr.start, .end = PG_ALIGN(old + increment) });

    return old;
}

void sys_brk(va_t new_end)
{
    struct vm_map *data;
    struct task *t = cpu_get_local()->current;

    new_end = PG_ALIGN(new_end);

    kp(KP_TRACE, "%p: brk: %p\n", t, new_end);

    data = t->addrspc->data;

    /* Check if we have a data segment, and create a new one after the end of
     * the code segment if we don't */
    if (!data) {
        create_data(t->addrspc, new_end);
        return ;
    }

    /* Else expand the current data segment */
    if (data->addr.start >= new_end)
        vm_map_resize(data, (struct vm_region) { .start = data->addr.start, .end = new_end });
}

