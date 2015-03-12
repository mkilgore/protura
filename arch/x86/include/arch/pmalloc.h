#ifndef INCLUDE_ARCH_PMALLOC_H
#define INCLUDE_ARCH_PMALLOC_H

#include <protura/types.h>

enum {
    PMAL_KERNEL = (1 << 0),
    PMAL_HIGH = (1 << 1),
};

pa_t pmalloc_page_alloc(int flags);
pa_t pmalloc_pages_alloc(int flags, size_t count);

void pmalloc_page_free(pa_t page);
void pmalloc_pages_free(pa_t page, size_t count);

void pmalloc_page_set(pa_t page);

void pmalloc_init(va_t kernel_end, pa_t last_physical_address);

#endif
