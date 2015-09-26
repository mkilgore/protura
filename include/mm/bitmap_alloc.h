#ifndef INCLUDE_MM_BITMAP_ALLOC_H
#define INCLUDE_MM_BITMAP_ALLOC_H

#include <protura/types.h>

struct bitmap_alloc {
    uintptr_t addr_start;

    uint32_t page_count;
    size_t page_size;

    uint32_t bitmap_size;
    uint8_t *bitmap;

    uint32_t last_found;
};

uintptr_t bitmap_alloc_get_page(struct bitmap_alloc *);
uintptr_t bitmap_alloc_get_pages(struct bitmap_alloc *, int pages);

void bitmap_alloc_free_page(struct bitmap_alloc *, uintptr_t);
void bitmap_alloc_free_pages(struct bitmap_alloc *, uintptr_t, int pages);
void bitmap_disp(struct bitmap_alloc *bitmap);

#endif
