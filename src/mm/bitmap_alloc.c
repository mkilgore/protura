/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/string.h>
#include <protura/debug.h>
#include <mm/bitmap_alloc.h>

#define bs(bitm) (sizeof(*(bitm)->bitmap))

#define bitmap_get_bit(bitm, n) (((bitm)->bitmap[(n) / 8] & (1 << ((n) % 8))) == (1 << ((n) % 8)))
#define bitmap_set_bit(bitm, n, b)                                      \
    do {                                                                \
        if (b)                                                          \
            (bitm)->bitmap[(n) / 8] |= (1 << ((n) % 8));                \
        else                                                            \
            (bitm)->bitmap[(n) / 8] &= ~(1 << ((n) % 8));               \
    } while (0)

uintptr_t bitmap_alloc_get_page(struct bitmap_alloc *bitmap)
{
    int bits = bitmap->bitmap_size * bs(bitmap);
    int i;
    for (i = 0; i < bits; i++) {
        if (!bitmap_get_bit(bitmap, i)) {
            bitmap_set_bit(bitmap, i, 1);
            return bitmap->addr_start + (i * bitmap->page_size);
        }
    }

    return 0;
}

static char buf[5000];

void bitmap_disp(struct bitmap_alloc *bitmap)
{
    int i;


    for (i = 0; i < bitmap->bitmap_size * 8 && i < sizeof(buf) - 1; i++) {
        if (bitmap_get_bit(bitmap, i))
            buf[i] = '1';
        else
            buf[i] = '0';
    }

    buf[i] = '\0';

    kprintf("Bitmap: %s\n", buf);
}

uintptr_t bitmap_alloc_get_pages(struct bitmap_alloc *bitmap, int pages)
{
    int bits = bitmap->bitmap_size * bs(bitmap);
    int i;
    for (i = 0; i < bits; i++) {
        if (!bitmap_get_bit(bitmap, i)) {
            int k;
            for (k = 1; k < pages; k++)
                if (bitmap_get_bit(bitmap, k + i))
                    break;
            if (k != pages)
                continue;

            for (k = 0; k < pages; k++)
                bitmap_set_bit(bitmap, k + i, 1);

            return bitmap->addr_start + (i * bitmap->page_size);
        }
    }

    return 0;
}

void bitmap_alloc_free_page(struct bitmap_alloc *bitmap, uintptr_t page)
{
    int page_n = (page - bitmap->addr_start) / bitmap->page_size;
    bitmap_set_bit(bitmap, page_n, 0);
}

void bitmap_alloc_free_pages(struct bitmap_alloc *bitmap, uintptr_t page, int pages)
{
    while (pages--)
        bitmap_alloc_free_page(bitmap, page + pages * bitmap->page_size);
}

