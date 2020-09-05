/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_PAGING_H
#define INCLUDE_ARCH_PAGING_H

#define PG_SHIFT (12)

#define PG_SIZE (1 << PG_SHIFT)

#define PDE_PRESENT        0x001
#define PDE_WRITABLE       0x002
#define PDE_USER           0x004
#define PDE_WRITE_THROUGH  0x008
#define PDE_CACHE_DISABLE  0x010
#define PDE_ACCESSED       0x020
#define PDE_PAGE_SIZE      0x080
#define PDE_GLOBAL         0x100
#define PDE_RESERVED       0x040

#define PTE_PRESENT        0x001
#define PTE_WRITABLE       0x002
#define PTE_USER           0x004
#define PTE_WRITE_THROUGH  0x008
#define PTE_CACHE_DISABLE  0x010
#define PTE_ACCESSED       0x020
#define PTE_DIRTY          0x040
#define PTE_PAT_BIT_3      0x080
#define PTE_GLOBAL         0x100

#define PTE_PAT_BIT_1      0x008 /* Write Through */
#define PTE_PAT_BIT_2      0x010 /* Cache Disable */

/* Align to a power of two */
#define ALIGN_2(v, a) ((typeof(v))(((uintptr_t)(v) + (a) - 1) & ~(a - 1)))
#define PG_ALIGN(v) ALIGN_2(v, PG_SIZE)

#define ALIGN_2_DOWN(v, a) ((typeof(v))(((uintptr_t)(v) & ~(a - 1))))
#define PG_ALIGN_DOWN(v) ALIGN_2_DOWN(v, PG_SIZE)

#define PAGING_DIR_INDEX_MASK 0x3FF
#define PAGING_TABLE_INDEX_MASK 0x3FF

#define PAGING_DIR_INDEX(val) (((uintptr_t)(val) >> 22) & PAGING_DIR_INDEX_MASK)
#define PAGING_TABLE_INDEX(val) (((uintptr_t)(val) >> 12) & PAGING_TABLE_INDEX_MASK)

#define PAGING_DIR_INDEX_WO(val) (((val) >> 22) & PAGING_DIR_INDEX_MASK)
#define PAGING_TABLE_INDEX_WO(val) (((val) >> 12) & PAGING_TABLE_INDEX_MASK)

#define PAGING_MAKE_DIR_INDEX(val) ((val) << 22)
#define PAGING_MAKE_TABLE_INDEX(val) ((val) << 12)

#define PAGING_FRAME_MASK 0xFFFFF000
#define PAGING_ATTRS_MASK 0x00000FFF

#define PAGING_FRAME(entry) ((entry) & PAGING_FRAME_MASK)

#ifndef ASM

#include <protura/types.h>
#include <protura/multiboot.h>
#include <arch/cpuid.h>

#include <protura/stddef.h>

static __always_inline void paging_disable(void)
{
    asm volatile("movl %%cr0, %%eax;"
                 "andl %0, %%eax;"
                 "movl %%eax, %%cr0":: "n" (~CR0_PG): "memory", "eax");
}

static __always_inline void paging_enable(void)
{
    asm volatile("movl %%cr0, %%eax;"
                 "orl %0, %%eax;"
                 "movl %%eax, %%cr0":: "n" (CR0_PG): "memory", "eax");
}

static __always_inline pa_t get_current_page_directory(void)
{
    uintptr_t pdir;
    asm volatile("movl %%cr3, %0": "=r" (pdir));
    return pdir;
}

static __always_inline void set_current_page_directory(pa_t page_directory)
{
    asm volatile("movl %0, %%eax\n"
                 "movl %%eax, %%cr3":: "r" (page_directory):"eax", "memory");
}

static __always_inline void flush_tlb_single(va_t addr)
{
    asm volatile("invlpg (%0)"::"r" (addr): "memory");
}

static inline pn_t __PA_TO_PN(pa_t addr)
{
    return addr >> PG_SHIFT;
}

static inline pa_t __PN_TO_PA(pn_t pn)
{
    return pn << PG_SHIFT;
}

void paging_setup_kernelspace(void);

uintptr_t paging_get_phys(va_t virtaddr);
void paging_dump_directory(pa_t dir);

#endif

#endif
