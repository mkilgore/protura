/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/string.h>
#include <protura/snprintf.h>
#include <protura/debug.h>
#include <protura/bits.h>
#include <protura/limits.h>
#include <mm/memlayout.h>
#include <mm/palloc.h>

#include <arch/spinlock.h>
#include <arch/paging.h>
#include <mm/slab.h>

struct page_frame_obj_empty {
    struct page_frame_obj_empty *next;
};

struct slab_page_frame {
    struct slab_page_frame *next;
    void *first_addr;
    int page_index_size;
    int object_count;
    struct page_frame_obj_empty *freelist;
};

void __slab_info(struct slab_alloc *slab, char *buf, size_t buf_size)
{
    struct slab_page_frame *frame;

    snprintf(buf, buf_size, "slab %s:\n", slab->slab_name);
    for (frame = slab->first_frame; frame != NULL; frame = frame->next)
        snprintf(buf, buf_size, "  frame (%p): %d objects\n", frame, frame->object_count);
}

static struct slab_page_frame *__slab_frame_new(struct slab_alloc *slab, unsigned int flags)
{
    char *obj;
    struct page_frame_obj_empty **current;
    struct slab_page_frame *newframe;

    /* 5 is just a random size - Since it's an index, that means we grab 32
     * pages at a time for each frame. */
    int i, page_index = 5;


    kprintf("Calling palloc with %d, %d\n", flags, page_index);
    newframe = palloc_multiple(page_index, flags);
    kprintf("New frame for slab: %p\n", newframe);

    if (!newframe)
        return NULL;

    newframe->page_index_size = page_index;

    newframe->next = NULL;

    newframe->first_addr = ALIGN_2(((char *)newframe) + sizeof(*newframe), slab->object_size);
    newframe->object_count = (((char *)newframe + PG_SIZE * (1 << newframe->page_index_size)) - (char *)newframe->first_addr) / slab->object_size;

    current = &newframe->freelist;
    obj = newframe->first_addr;
    for (i = 0; i < newframe->object_count; i++,
                                            obj = ALIGN_2(obj + slab->object_size, slab->object_size),
                                            current = &((*current)->next))
        *current = (struct page_frame_obj_empty *)obj;

    *current = NULL;


    kprintf("slab: %s: New page %p, %d objects\n", slab->slab_name, newframe, newframe->object_count);
    return newframe;
}

static void __slab_frame_free(struct slab_page_frame *frame)
{
    pfree_multiple(frame, frame->page_index_size);
}

static void *__slab_frame_object_alloc(struct slab_page_frame *frame)
{
    struct page_frame_obj_empty *obj, *next;
    if (!frame->freelist)
        return NULL;

    obj = frame->freelist;
    next = frame->freelist->next;

    frame->freelist = next;
    return obj;
}

static void __slab_frame_object_free(struct slab_page_frame *frame, void *obj)
{
    struct page_frame_obj_empty *new = obj, **current;

    for (current = &frame->freelist; *current; current = &(*current)->next) {
        if (obj <= (void *)(*current) || !*current) {
            new->next = *current;
            *current = new;
            return ;
        }
    }
}

void *__slab_malloc(struct slab_alloc *slab, unsigned int flags)
{
    struct slab_page_frame **frame = &slab->first_frame;

    for (frame = &slab->first_frame; *frame; frame = &((*frame)->next))
        if ((*frame)->freelist)
            return __slab_frame_object_alloc(*frame);

    *frame = __slab_frame_new(slab, flags);

    if (*frame)
        return __slab_frame_object_alloc(*frame);
    else
        return NULL;
}

int __slab_has_addr(struct slab_alloc *slab, void *addr)
{
    struct slab_page_frame *frame;
    for (frame = slab->first_frame; frame; frame = frame->next)
        if (addr >= (void *)frame && addr < (void *)frame + PG_SIZE)
            return 0;

    return 1;
}

void __slab_free(struct slab_alloc *slab, void *obj)
{
    struct slab_page_frame *frame;
    for (frame = slab->first_frame; frame; frame = frame->next) {
        if (obj >= (void *)frame && obj < (void *)frame + PG_SIZE) {
            __slab_frame_object_free(frame, obj);
            return ;
        }
    }

    panic("slab: Error! Attempted to free address %p, not in slab %s\n", obj, slab->slab_name);
}

void __slab_clear(struct slab_alloc *slab)
{
    struct slab_page_frame *frame, *next;
    for (frame = slab->first_frame; frame; frame = next) {
        next = frame->next;
        __slab_frame_free(frame);
    }
}

void *slab_malloc(struct slab_alloc *slab, unsigned int flags)
{
    void *ret;

    using_spinlock(&slab->lock)
        ret = __slab_malloc(slab, flags);

    return ret;
}

int slab_has_addr(struct slab_alloc *slab, void *addr)
{
    int ret;

    using_spinlock(&slab->lock)
        ret = __slab_has_addr(slab, addr);

    return ret;
}

void slab_free(struct slab_alloc *slab, void *obj)
{
    using_spinlock(&slab->lock)
        __slab_free(slab, obj);
}

void slab_clear(struct slab_alloc *slab)
{
    using_spinlock(&slab->lock)
        __slab_clear(slab);
}

