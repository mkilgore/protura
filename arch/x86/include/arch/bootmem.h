#ifndef INCLUDE_ARCH_BOOTMEM_H
#define INCLUDE_ARCH_BOOTMEM_H

#include <arch/types.h>

void bootmem_init(pa_t kernel_end);
void bootmem_transfer_to_pmalloc(void);

/* Only does pages */
pa_t bootmem_alloc_page(void);
pa_t bootmem_alloc_pages(int pages);
void bootmem_free_page(pa_t page);
void bootmem_free_pages(pa_t page, int pages);

#endif
