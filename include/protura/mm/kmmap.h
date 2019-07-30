#ifndef INCLUDE_PROTURA_MM_KMMAP_H
#define INCLUDE_PROTURA_MM_KMMAP_H

#include <protura/types.h>
#include <protura/mm/palloc.h>

#define KMAP_PAGES ((CONFIG_KERNEL_KMAP_SIZE) >> PG_SHIFT)

void *kmmap(pa_t address, size_t len, flags_t vm_flags);
void kmunmap(void *);

#endif
