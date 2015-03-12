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

#include <arch/paging.h>
#include <arch/pmalloc.h>
#include <mm/slab.h>

struct slab_page_frame {
    struct slab_page_frame *next;
    void *first_addr;
    int object_count;
    uint8_t flags[PG_SIZE / 8 / 4];
};

void slab_info(struct slab_alloc *slab, char *buf, size_t buf_size)
{
    struct slab_page_frame *frame;

    snprintf(buf, buf_size, "slab %s:\n", slab->slab_name);
    for (frame = slab->first_frame; frame != NULL; frame = frame->next)
        snprintf(buf, buf_size, "  frame (%p): %d objects\n", frame, frame->object_count);
}

void *slab_malloc(struct slab_alloc *slab)
{
    struct slab_page_frame **frame = &slab->first_frame;
    struct slab_page_frame *newframe;

    for (frame = &slab->first_frame; *frame; frame = &((*frame)->next)) {
        int i, count = (*frame)->object_count;
        for (i = 0; i < count; i++) {
            if (!bit_get((*frame)->flags, i)) {
                bit_set((*frame)->flags, i, 1);
                return (*frame)->first_addr + i * slab->object_size;
            }
        }
    }

    newframe = P2V(pmalloc_page_alloc(PMAL_KERNEL));

    newframe->next = NULL;

    newframe->first_addr = ALIGN(((char *)newframe) + sizeof(*newframe), slab->object_size);
    newframe->object_count = (((char *)newframe + PG_SIZE) - (char *)newframe->first_addr) / slab->object_size;

    kprintf("slab: %s: New page %p, %d objects\n", slab->slab_name, newframe, newframe->object_count);

    *frame = newframe;

    bit_set(newframe->flags, 1, 1);
    return newframe->first_addr;
}

int slab_has_addr(struct slab_alloc *slab, void *addr)
{
    struct slab_page_frame *frame;
    for (frame = slab->first_frame; frame; frame = frame->next)
        if (addr >= (void *)frame && addr < (void *)frame + PG_SIZE)
            return 0;

    return 1;
}

void slab_free(struct slab_alloc *slab, void *obj)
{
    struct slab_page_frame *frame;
    for (frame = slab->first_frame; frame; frame = frame->next) {
        if (obj >= (void *)frame && obj < (void *)frame + PG_SIZE) {
            int obj_n = (uintptr_t)(obj - frame->first_addr) / slab->object_size;
            bit_set(frame->flags, obj_n, 0);
            return ;
        }
    }

    panic("slab: Error! Attempted to free address %p, not in slab %s\n", obj, slab->slab_name);
}

void slab_clear(struct slab_alloc *slab)
{
    struct slab_page_frame *frame, *next;
    for (frame = slab->first_frame; frame; frame = next) {
        next = frame->next;
        pmalloc_page_free(V2P(frame));
    }
}

