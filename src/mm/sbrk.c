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
#include <protura/task.h>
#include <protura/mm/palloc.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/memlayout.h>
#include <protura/mm/vm.h>

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
