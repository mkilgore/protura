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

/* An important note to readers:
 *
 * kmalloc doesn't actually do any locking. This is because slab are locked
 * individually, inside of slab_malloc/slab_free/etc. , and the relevant
 * 'kmalloc_slabs' array information is never changed so kmalloc/kfree can all
 * read it at the same time.
 *
 * Slabs can be added and removed from this list as wanted. Slab's have to have
 * a size of a power-of-two.
 */

static struct slab_alloc kmalloc_slabs[] = {
    SLAB_ALLOC_INIT("kmalloc_32", 32),
    SLAB_ALLOC_INIT("kmalloc_64", 64),
    SLAB_ALLOC_INIT("kmalloc_128", 128),
    SLAB_ALLOC_INIT("kmalloc_256", 256),
    SLAB_ALLOC_INIT("kmalloc_512", 512),
    SLAB_ALLOC_INIT("kmalloc_1024", 1024),
    SLAB_ALLOC_INIT("kmalloc_2048", 2048),
    { .slab_name = NULL }
};

void kmalloc_init(void)
{

}

void kmalloc_oom(void)
{
    struct slab_alloc *slab;
    for (slab = kmalloc_slabs; slab->slab_name; slab++)
        slab_oom(slab);
}

void *kmalloc(size_t size, int flags)
{
    struct slab_alloc *slab;
    for (slab = kmalloc_slabs; slab->slab_name; slab++)
        if (size <= slab->object_size)
            return slab_malloc(slab, flags);

    panic("kmalloc: Size %d is to big for kmalloc! Use palloc to get whole-pages instead\n", size);
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

