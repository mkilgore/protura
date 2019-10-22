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
#include <protura/mm/memlayout.h>
#include <protura/mm/palloc.h>

#include <arch/spinlock.h>
#include <arch/paging.h>
#include <protura/mm/slab.h>

#ifdef CONFIG_KERNEL_LOG_SLAB
# define kp_slab(s, str, ...) kp(KP_NORMAL, "SLAB %s: " str, (s)->slab_name, ## __VA_ARGS__)
#else
# define kp_slab(s, str, ...) do { ; } while (0)
#endif

#define SLAB_POISON (0x0EADBEAF)

struct page_frame_obj_empty {
    struct page_frame_obj_empty *next;
};

struct slab_page_frame {
    struct slab_page_frame *next;
    void *first_addr;
    void *last_addr;
    int page_index_size;
    int object_count;
    int free_object_count;
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

    int i, page_index = CONFIG_KERNEL_SLAB_ORDER;

    kp_slab(slab, "Calling palloc with %d, %d\n", flags, page_index);
    newframe = palloc_va(page_index, flags);
    kp_slab(slab, "New frame for slab: %p\n", newframe);

    if (!newframe)
        return NULL;

    newframe->page_index_size = page_index;

    newframe->next = NULL;

    newframe->first_addr = ALIGN_2(((char *)newframe) + sizeof(*newframe), slab->object_size);
    newframe->object_count = (((char *)newframe + PG_SIZE * (1 << newframe->page_index_size)) - (char *)newframe->first_addr) / slab->object_size;
    newframe->last_addr = newframe->first_addr + newframe->object_count * slab->object_size;
    newframe->free_object_count = newframe->object_count;

    current = &newframe->freelist;
    obj = newframe->first_addr;
    int count = 0;
    for (i = 0; i < newframe->object_count; i++,
                                            obj = ALIGN_2(obj + slab->object_size, slab->object_size),
                                            current = &((*current)->next)) {

        uint32_t *poison = (uint32_t *)obj;
        int k = 0;
        for (; k < slab->object_size / 4; k++)
            poison[k] = SLAB_POISON;

        *current = (struct page_frame_obj_empty *)obj;

        count++;

        kp_slab(slab, "__slab_frame_new: %p\n", obj);
    }

    *current = NULL;

    return newframe;
}

static void __slab_frame_free(struct slab_alloc *slab, struct slab_page_frame *frame)
{
    kp_slab(slab, "Calling pfree with %p, %d\n", frame, frame->page_index_size);
    pfree_va(frame, frame->page_index_size);
}

static void *__slab_frame_object_alloc(struct slab_alloc *slab, struct slab_page_frame *frame)
{
    struct page_frame_obj_empty *obj, *next;
    if (!frame->freelist)
        return NULL;

    obj = frame->freelist;

    /* Skip the valid page_frame entry, and verify the rest of the entry is equal to the poison value */
    uint32_t *poison = (uint32_t *)(obj + 1);
    size_t poison_count = (slab->object_size - sizeof(*obj)) / 4;
    int k = 0;
    for (; k < poison_count; k++) {
        if (poison[k] != SLAB_POISON) {
            kp(KP_ERROR, "SLAB %s: %p: POISON IS INVALID, offset: %zd!!!!", slab->slab_name, obj, k * 4 + sizeof(*obj));
            dump_stack();
            break;
        }
    }

    next = frame->freelist->next;

    frame->freelist = next;
    frame->free_object_count--;

    kp_slab(slab, "__slab_frame_object_alloc: %p\n", obj);
    return obj;
}

static void __slab_frame_object_free(struct slab_alloc *slab, struct slab_page_frame *frame, void *obj)
{
    struct page_frame_obj_empty *new = obj, **current;

    for (current = &frame->freelist; current; current = &(*current)->next) {
        if (obj <= (void *)(*current) || !*current) {
            uint32_t *poison = (uint32_t *)new;
            int k = 0;
            for (; k < slab->object_size / 4; k++)
                poison[k] = SLAB_POISON;

            new->next = *current;

            *current = new;
            frame->free_object_count++;
            return ;
        }
    }

    kp(KP_ERROR, "%p was freed, but is not in frame %p!!!!!\n", obj, frame);
}

void *__slab_malloc(struct slab_alloc *slab, unsigned int flags)
{
    struct slab_page_frame **frame = &slab->first_frame;

    for (frame = &slab->first_frame; *frame; frame = &((*frame)->next)) {
        if ((*frame)->free_object_count && !(*frame)->freelist) {
            kp(KP_ERROR, "free_object_count and freelist do not agree! %s, %d, %p\n", slab->slab_name, (*frame)->free_object_count, (*frame)->freelist);
            break;
        }

        if ((*frame)->free_object_count)
            return __slab_frame_object_alloc(slab, *frame);
    }

    *frame = __slab_frame_new(slab, flags);

    if (*frame)
        return __slab_frame_object_alloc(slab, *frame);
    else
        return NULL;
}

int __slab_has_addr(struct slab_alloc *slab, void *addr)
{
    struct slab_page_frame *frame;
    for (frame = slab->first_frame; frame; frame = frame->next)
        if (addr >= frame->first_addr && addr < frame->last_addr)
            return 0;

    return 1;
}

void __slab_free(struct slab_alloc *slab, void *obj)
{
    kp_slab(slab, "free %p\n", obj);
    struct slab_page_frame *frame;
    for (frame = slab->first_frame; frame; frame = frame->next)
        if (obj >= frame->first_addr && obj < frame->last_addr)
            return __slab_frame_object_free(slab, frame, obj);

    panic("slab: Error! Attempted to free address %p, not in slab %s\n", obj, slab->slab_name);
}

void __slab_clear(struct slab_alloc *slab)
{
    struct slab_page_frame *frame, *next;
    for (frame = slab->first_frame; frame; frame = next) {
        next = frame->next;
        __slab_frame_free(slab, frame);
    }
}

void __slab_oom(struct slab_alloc *slab)
{
    struct slab_page_frame **prev = &slab->first_frame, *frame, *next;
    for (frame = slab->first_frame; frame; frame = next) {
        next = frame->next;

        if (frame->free_object_count == frame->object_count) {
            __slab_frame_free(slab, frame);
            *prev = next;
        } else {
            prev = &frame->next;
        }
    }
}

void *slab_malloc(struct slab_alloc *slab, unsigned int flags)
{
    void *ret;

    using_spinlock(&slab->lock)
        ret = __slab_malloc(slab, flags);

    kp_slab(slab, "malloc new: %p\n", ret);
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

void slab_oom(struct slab_alloc *slab)
{
    using_spinlock(&slab->lock)
        __slab_oom(slab);
}

#ifdef CONFIG_KERNEL_TESTS
# include "slab_test.c"
#endif
