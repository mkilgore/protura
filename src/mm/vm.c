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
#include <mm/palloc.h>
#include <mm/kmalloc.h>
#include <mm/memlayout.h>
#include <mm/vm.h>


void address_space_map_vm_entry(struct address_space *addrspc, va_t virtual, pa_t physical, flags_t vm_flags)
{
    arch_address_space_map_vm_entry(addrspc, virtual, physical, vm_flags);
}

void address_space_map_vm_map(struct address_space *addrspc, struct vm_map *map)
{
    arch_address_space_map_vm_map(addrspc, map);
}

void address_space_unmap_vm_entry(struct address_space *addrspc, va_t virtual)
{
    arch_address_space_unmap_vm_map(addrspc, virtual);
}

void address_space_unmap_vm_map(struct address_space *addrspc, struct vm_map *map)
{
    arch_address_space_unmap_vm_map(addrspc, map);
}

void address_space_copy(struct address_space *new, struct address_space *old)
{
    arch_address_space_copy(new, old);
}

void address_space_vm_map_add(struct address_space *addrspc, struct vm_map *map)
{
    list_add(&addrspc->vm_maps, &map->address_space_entry);
    map->owner = addrspc;
    address_space_map_vm_map(addrspc, map);
}

void address_space_vm_map_remove(struct address_space *addrspc, struct vm_map *map)
{
    list_del(&map->address_space_entry);
    map->owner = NULL;
    address_space_unmap_vm_map(addrspc, map);
}

static void vm_map_resize_start(struct vm_map *map, va_t new_start)
{
    if (map->addr.start > new_start) {
        /* Count the number of new pages needed - Note that because
         * map->addr.start, and new_start are not required to be page aligned,
         * this number may actually be zero if both addresses are in the same
         * page. Thus, we align both addresses to the containing page. */
        int new_pages = (PG_ALIGN_DOWN(map->addr.start) - PG_ALIGN_DOWN(new_start)) / PG_SIZE;
        int i;
        va_t new_addr = PG_ALIGN_DOWN(map->addr.start) - PG_SIZE;

        for (i = 0; i < new_pages; i++, new_addr -= PG_SIZE) {
            struct page *p = palloc(0, PAL_KERNEL);

            list_add(&map->page_list, &p->page_list_node);
            address_space_map_vm_entry(map->owner, new_addr, page_to_pa(p), map->flags);
        }
    } else {
        int old_pages = (PG_ALIGN_DOWN(new_start) - PG_ALIGN_DOWN(map->addr.start)) / PG_SIZE;
        int i;
        va_t new_addr = PG_ALIGN_DOWN(map->addr.start);

        for (i = 0; i < old_pages; i++, new_addr += PG_SIZE) {
            struct page *p = list_take_first(&map->page_list, struct page, page_list_node);

            address_space_unmap_vm_entry(map->owner, new_addr);
            pfree(p, 0);
        }
    }

    map->addr.start = new_start;
}

static void vm_map_resize_end(struct vm_map *map, va_t new_end)
{
    if (new_end > map->addr.end) {
        int new_pages = (PG_ALIGN(new_end) - PG_ALIGN(map->addr.end)) / PG_SIZE;
        int i;
        va_t cur_page = PG_ALIGN(map->addr.end);

        for (i = 0; i < new_pages; i++, cur_page += PG_SIZE) {
            struct page *p = palloc(0, PAL_KERNEL);

            list_add_tail(&map->page_list, &p->page_list_node);
            address_space_map_vm_entry(map->owner, cur_page, page_to_pa(p), map->flags);
        }
    } else {
        int old_pages = (PG_ALIGN(map->addr.end) - PG_ALIGN(new_end)) / PG_SIZE;
        int i;
        va_t cur_page = PG_ALIGN(map->addr.end) - PG_SIZE;

        for (i = 0; i < old_pages; i++, cur_page -= PG_SIZE) {
            struct page *p = list_take_last(&map->page_list, struct page, page_list_node);

            address_space_unmap_vm_entry(map->owner, cur_page);
            pfree(p, 0);
        }
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

int user_check_region(const va_t ptr, size_t size, flags_t vm_flags)
{
    struct task *t = cpu_get_local()->current;
    struct address_space *addrspc = t->addrspc;
    struct vm_map *cur;

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

int user_check_strn(const va_t ptr, size_t size, flags_t vm_flags)
{
    struct task *t = cpu_get_local()->current;
    struct address_space *addrspc = t->addrspc;
    struct vm_map *cur;

    list_foreach_entry(&addrspc->vm_maps, cur, address_space_entry) {
        if (cur->addr.start <= ptr) {
            va_t end = ptr + size;
            if (end >= cur->addr.end)
                end = cur->addr.end - 1;

            if ((cur->flags & vm_flags) != vm_flags)
                return -EFAULT;

            char *c;
            for (c = end; c >= (char *)ptr; c--)
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

    if (new_end > data_start) {
        data = kmalloc(sizeof(*data), PAL_KERNEL);
        vm_map_init(data);

        data->addr.start = data_start;
        data->addr.end = new_end;

        flag_set(&data->flags, VM_MAP_WRITE);
        flag_set(&data->flags, VM_MAP_READ);

        palloc_unordered(&data->page_list, (PG_ALIGN(new_end) - data_start) / PG_SIZE, PAL_KERNEL);

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
        va_t data_start = PG_ALIGN(t->addrspc->code->addr.end);

        create_data(t->addrspc, data_start + increment);

        if (t->addrspc->data)
            return data_start;
        else
            return NULL;
    }

    old = data->addr.end;

    vm_map_resize(data, (struct vm_region) { .start = data->addr.start, .end = old + increment });

    return old;
}

void sys_brk(va_t new_end)
{
    struct vm_map *data;
    struct task *t = cpu_get_local()->current;

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

