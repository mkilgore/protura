/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/string.h>
#include <protura/debug.h>

#include <mm/memlayout.h>
#include <mm/kmalloc.h>
#include <mm/slab.h>

static struct slab_alloc kmalloc_slabs[] = {
    { .slab_name = "kmalloc_32", .object_size = 32 },
    { .slab_name = "kmalloc_64", .object_size = 64 },
    { .slab_name = "kmalloc_128", .object_size = 128 },
    { .slab_name = "kmalloc_256", .object_size = 256 },
    { .slab_name = "kmalloc_512", .object_size = 512 },
    { .slab_name = "kmalloc_1024", .object_size = 1024 },
    { .slab_name = "kmalloc_2048", .object_size = 2048 },
    { .slab_name = NULL }
};

void kmalloc_init(void)
{

}

void *kmalloc(size_t size, int flags)
{
    struct slab_alloc *slab;
    for (slab = kmalloc_slabs; slab->slab_name; slab++)
        if (size <= slab->object_size)
            return slab_malloc(slab);

    panic("kmalloc: Size %d is to big for kmalloc! Use pmalloc to get whole-pages instead\n", size);
}

void *kzalloc(size_t size, int flags)
{
    void *m = kmalloc(size, flags);
    if (m)
        memset(m, 0, size);
    return m;
}

size_t ksize(void *p)
{
    struct slab_alloc *slab;
    for (slab = kmalloc_slabs; slab->slab_name; slab++)
        if (slab_has_addr(slab, p) == 0)
            return slab->object_size;

    return 0;
}

void kfree(void *p)
{
    struct slab_alloc *slab;
    for (slab = kmalloc_slabs; slab->slab_name; slab++) {
        if (slab_has_addr(slab, p) == 0) {
            slab_free(slab, p);
            return ;
        }
    }

    panic("kmalloc: Error! Addr %p was not found in kmalloc's memory space!\n", p);
}

