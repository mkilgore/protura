/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/multiboot.h>
#include <protura/string.h>
#include <protura/debug.h>
#include <mm/memlayout.h>

#include <arch/kbrk.h>
#include <arch/paging.h>

struct page_entry {
    struct page_entry *next;
    uintptr_t phys_start;
    uint32_t pages;
    uint8_t bitmap[];
};

static struct {
    struct page_entry *first;
} allocator;

/* A zeroed page_directory we can use for our initial table */
__align(0x1000)
struct page_table_ptr kernel_dir[1024] = {
    { .entry = 0 }
};

void page_directory_init(void)
{

}

void paging_init(struct multiboot_info *info)
{
    struct multiboot_memmap *mmap = (struct multiboot_memmap *)info->mmap_addr;
    struct page_entry *ent;
    size_t size;
    uint64_t addr;

    for (; (uint32_t)mmap < info->mmap_addr + info->mmap_length
         ; mmap = (struct multiboot_memmap *) ((uint32_t)mmap + mmap->size + sizeof(uint32_t))) {
        kprintf("mmap: %llx to %llx, type: %d\n", mmap->base_addr,
                mmap->base_addr + mmap->length, mmap->type);

        /* A type of non-one means it's not usable memory - just ignore it */
        if (mmap->type != 1)
            continue;

        /* A small hack - We skip the map for the first MB since we map it separate */
        if (mmap->base_addr == 0)
            continue;

        addr = PG_ALIGN(mmap->base_addr);

        mmap->length -= addr - mmap->base_addr;
        mmap->length &= 0xFFFFF000; /* Page align length */
        mmap->base_addr = addr;

        size = sizeof(struct page_entry) + ((mmap->length >> 12) >> 3) + 1;

        ent = kbrk(size, alignof(struct page_entry));

        ent->next = allocator.first;
        ent->phys_start = mmap->base_addr;
        ent->pages = mmap->length >> 12;

        allocator.first = ent;

        memset(ent->bitmap, 0, ent->pages >> 3);
    }
    return ;
}

uintptr_t get_page(void)
{
    struct page_entry *ent = allocator.first;
    uint8_t mask;
    uint32_t cur_byte, max = ent->pages >> 3;
    uint32_t page_n = 0;

    for (; ent != NULL; ent = ent->next) {
        for (cur_byte = 0; cur_byte < max; cur_byte++) {
            for (mask = 0; mask != 8; mask++) {
                if ((ent->bitmap[cur_byte] & (1 << mask)) == 0) {
                    page_n = (cur_byte << 3) + mask;
                    ent->bitmap[cur_byte] |= (1 << mask);

                    goto eloop;
                }
            }
        }
    }

eloop:

    if (ent == NULL)
        return 0;

    return ent->phys_start + (page_n << 12);
}

void free_page(uintptr_t p)
{
    struct page_entry *ent = allocator.first;
    uint8_t mask;
    uint32_t byte;
    uintptr_t offset;

    for (; ent != NULL; ent = ent->next)
        if (ent->phys_start <= p && (ent->phys_start + (ent->pages << 12)) >= p)
            break;

    if (ent == NULL)
        return ;

    offset = p - ent->phys_start;
    byte = offset >> 15;
    mask = (offset >> 12) & 0x7;

    ent->bitmap[byte] &= ~(1 << mask);
}

