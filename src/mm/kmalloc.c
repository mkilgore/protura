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

#include <protura/mm/memlayout.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/slab.h>

/* An important note to readers:
 *
 * kmalloc doesn't actually do any global locking. This is because slab are
 * locked individually, inside of slab_malloc/slab_free/etc. , and the relevant
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
    SLAB_ALLOC_INIT("kmalloc_4096", 4096),
    { .slab_name = NULL }
};

/* For sizes larger than the slab allocators can supply, kmalloc() simply
 * allocates from palloc() directly. We store those allocations in a big list.
 * Very unoptimal, but there very few users of this functionality in the
 * kernel, most just call palloc() directly. */
struct large_alloc_desc {
    list_node_t node;
    struct page *pages;
    int order;
};

static spinlock_t large_alloc_lock = SPINLOCK_INIT();
static list_head_t large_alloc_list = LIST_HEAD_INIT(large_alloc_list);

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

    int pages = PG_ALIGN(size) / PG_SIZE;
    int order = pages_to_order(pages);

    /* This is only one level of recursion because the descriptor is very small */
    struct large_alloc_desc *desc = kmalloc(sizeof(*desc), flags);
    if (!desc)
        return NULL;

    list_node_init(&desc->node);
    desc->order = order;
    desc->pages = palloc(order, flags);

    if (!desc->pages) {
        kfree(desc);
        return NULL;
    }

    using_spinlock(&large_alloc_lock)
        list_add_tail(&large_alloc_list, &desc->node);

    return desc->pages->virt;
}

size_t ksize(void *p)
{
    struct slab_alloc *slab;
    for (slab = kmalloc_slabs; slab->slab_name; slab++)
        if (slab_has_addr(slab, p) == 0)
            return slab->object_size;

    using_spinlock(&large_alloc_lock) {
        struct large_alloc_desc *desc;

        list_foreach_entry(&large_alloc_list, desc, node) {
            va_t start = desc->pages->virt;
            va_t end = desc->pages->virt + (1 << desc->order) * PG_SIZE;

            if (start <= p && p <= end)
                return (1 << desc->order) * PG_SIZE;
        }
    }

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

    struct large_alloc_desc *desc;

    using_spinlock(&large_alloc_lock) {
        list_foreach_entry(&large_alloc_list, desc, node) {
            va_t start = desc->pages->virt;
            va_t end = desc->pages->virt + (1 << desc->order) * PG_SIZE;

            if (start <= p && p <= end) {
                list_del(&desc->node);
                break;
            }
        }

        if (list_ptr_is_head(&desc->node, &large_alloc_list))
            desc = NULL;
    }

    if (desc) {
        pfree(desc->pages, desc->order);
        kfree(desc);
        return;
    }

    panic("kmalloc: Error! Addr %p was not found in kmalloc's memory space!\n", p);
}

char *kstrdup(const char *s, int flags)
{
    size_t len = strlen(s);
    char *buf = kmalloc(len + 1, flags);

    strcpy(buf, s);

    return buf;
}

char *kstrndup(const char *s, size_t n, int flags)
{
    size_t len = strlen(s);
    char *buf;

    if (len > n)
        len = n;

    buf = kmalloc(len + 1, flags);

    strncpy(buf, s, n);
    buf[len] = '\0';

    return buf;
}

