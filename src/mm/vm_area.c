/*
 * Copyright (C) 2017 Matt Kilgore
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
#include <protura/mutex.h>
#include <protura/mm/palloc.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/memlayout.h>
#include <protura/mm/vm.h>
#include <protura/mm/vm_area.h>
#include <protura/mm/kmmap.h>

/*
 * A lock covering vm_area_list and vm_area_mappings
 */
static mutex_t vm_area_list_lock = MUTEX_INIT(vm_area_list_lock);

/*
 * vm_area_list is a list of all the currently free vm_area's, sorted by size.
 */
static list_head_t vm_area_list = LIST_HEAD_INIT(vm_area_list);

/*
 * An array that represents which pages of virtual memory are currently used by
 * what `vm_area`s.
 *
 * This allows us to quickly take an address and find it's associated
 * `vm_area`, and also take a vm_area and quickly check the nearest vm_areas to
 * it for combining into larger areas.
 */
static struct vm_area *vm_area_mappings[KMAP_PAGES];

static int vm_area_to_index(void *p)
{
    p -= CONFIG_KERNEL_KMAP_START;
    return (uintptr_t)p >> PG_SHIFT;
}

static void __vm_area_add(struct vm_area *area)
{
    struct vm_area *vm;

    list_foreach_entry(&vm_area_list, vm, vm_area_entry) {
        if (vm->page_count > area->page_count) {
            list_add_tail(&vm->vm_area_entry, &area->vm_area_entry);
            return ;
        }
    }

    list_add_tail(&vm_area_list, &area->vm_area_entry);
}

/*
 * Takes a vm_area with a page_count larger then 'new_pages', and splits it
 * into two vm_areas, one with a page_count of 'new_pages', and one with the
 * rest, and returns the vm_area with a page_count of 'new_pages'.
 */
static struct vm_area *__vm_area_split(struct vm_area *area, int new_pages)
{
    int i;
    int start_index;
    struct vm_area *alloced_area = kzalloc(sizeof(*alloced_area), PAL_KERNEL);
    struct vm_area *new_area = alloced_area;
    vm_area_init(new_area);

    /* We choose to use the newly allocated vm_area as the area with the free
     * or the used pages depending on which requires us to modify the least
     * number of 'vm_area_mappings' entries.
     *
     * This is simply a matter of finding which vm_area will end-up with less pages. */

    if (area->page_count / 2 >= new_pages) {
        new_area->area = area->area + PG_SIZE * new_pages;
        new_area->page_count = area->page_count - new_pages;
        flag_set(&new_area->flags, VM_AREA_FREE);

        area->page_count = new_pages;
    } else {
        struct vm_area *tmp;

        new_area->area = area->area;
        new_area->page_count = new_pages;

        area->area = new_area->area + PG_SIZE * new_pages;
        area->page_count = area->page_count - new_pages;
        flag_set(&area->flags, VM_AREA_FREE);

        tmp = new_area;
        new_area = area;
        area = tmp;
    }

    start_index = vm_area_to_index(alloced_area->area);

    for (i = 0; i < alloced_area->page_count; i++)
        vm_area_mappings[i + start_index] = alloced_area;

    __vm_area_add(new_area);

    return area;
}

static struct vm_area *__vm_area_combine(struct vm_area *area)
{
    int again = 0;

    do {
        int i;
        int index = vm_area_to_index(area->area);
        struct vm_area *before = vm_area_mappings[index - 1];
        struct vm_area *after = vm_area_mappings[index + area->page_count];
        again = 0;

        if (index > 0 && flag_test(&before->flags, VM_AREA_FREE)) {
            int start_index;

            list_del(&before->vm_area_entry);


            /* Fancy swapping here is to reduce the number of vm_area_mappings
             * we have to modify down below */
            if (before->page_count < area->page_count) {
                area->area = before->area;
                area->page_count += before->page_count;
            } else {
                struct vm_area *tmp;
                before->page_count += area->page_count;

                tmp = area;
                area = before;
                before = tmp;
            }

            start_index = vm_area_to_index(before->area);

            for (i = 0; i < before->page_count; i++)
                vm_area_mappings[i + start_index] = area;

            kfree(before);
            again = 1;
        }

        if (index + area->page_count < KMAP_PAGES && flag_test(&after->flags, VM_AREA_FREE)) {
            int start_index;

            list_del(&after->vm_area_entry);

            /* Fancy swapping here is to reduce the number of vm_area_mappings
             * we have to modify down below */
            if (area->page_count < after->page_count) {
                area->page_count += after->page_count;
            } else {
                struct vm_area *tmp;

                after->area = area->area;
                after->page_count += area->page_count;

                tmp = area;
                area = after;
                after = tmp;
            }

            start_index = vm_area_to_index(after->area);

            for (i = 0; i < after->page_count; i++)
                vm_area_mappings[i + start_index] = area;

            kfree(after);
            again = 1;
        }

    } while (again);

    return area;
}

struct vm_area *vm_area_alloc(int pages)
{
    struct vm_area *area;

    using_mutex(&vm_area_list_lock) {
        list_foreach_entry(&vm_area_list, area, vm_area_entry) {
            if (area->page_count >= pages)
                break;
        }

        if (list_ptr_is_head(&vm_area_list, &area->vm_area_entry))
            return NULL;

        list_del(&area->vm_area_entry);

        if (area->page_count > pages)
            area = __vm_area_split(area, pages);

        flag_clear(&area->flags, VM_AREA_FREE);
    }

    return area;
}

void vm_area_free(struct vm_area *area)
{
    using_mutex(&vm_area_list_lock) {
        flag_set(&area->flags, VM_AREA_FREE);
        list_del(&area->vm_area_entry);

        area = __vm_area_combine(area);

        __vm_area_add(area);
    }
}

void vm_area_allocator_init(void)
{
    int i;

    struct vm_area *area = kmalloc(sizeof(*area), PAL_KERNEL);
    vm_area_init(area);

    area->area = (void *)CONFIG_KERNEL_KMAP_START;
    area->page_count = KMAP_PAGES;
    flag_set(&area->flags, VM_AREA_FREE);

    list_add(&vm_area_list, &area->vm_area_entry);

    for (i = 0; i < KMAP_PAGES; i++)
        vm_area_mappings[i] = area;
}

void *kmmap(pa_t address, size_t len, flags_t vm_flags)
{
    size_t addr_offset = address % PG_SIZE;
    pa_t pg_addr = address & ~PG_SIZE;
    int pages = PG_ALIGN(len + addr_offset) >> PG_SHIFT;
    struct vm_area *area;
    int i;

    kp(KP_NORMAL, "mem_map: %d pages, %p:%d\n", pages, (void *)address, len);

    area = vm_area_alloc(pages);

    for (i = 0; i < pages; i++)
        vm_area_map(area->area + i * PG_SIZE, pg_addr + i * PG_SIZE, vm_flags);

    return area->area + addr_offset;
}

void kmunmap(void *p)
{
    int index = vm_area_to_index(p);
    struct vm_area *area;
    int i;

    using_mutex(&vm_area_list_lock)
        area = vm_area_mappings[index];

    kp(KP_NORMAL, "mem_unmap: %p, %p:%d\n", p, area->area, area->page_count);

    for (i = 0; i < area->page_count; i++)
        vm_area_unmap(area->area + i * PG_SIZE);

    vm_area_free(area);
}

