#ifndef INCLUDE_ARCH_KMAP_H
#define INCLUDE_ARCH_KMAP_H

#include <protura/types.h>

/* Used to map a physical address into kernel address space */
void *kmap(pa_t addr);
void kunmap(void *);

#endif
