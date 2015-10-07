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

#define CR0_PE 0x00000001
#define CR0_MP 0x00000002
#define CR0_EM 0x00000004
#define CR0_TS 0x00000008
#define CR0_ET 0x00000010
#define CR0_NE 0x00000020
#define CR0_WP 0x00010000
#define CR0_AM 0x00040080
#define CR0_NW 0x20000000
#define CR0_CD 0x40000000
#define CR0_PG 0x80000000

#define CR4_PSE 0x00000010
#define CR4_GLOBAL 0x00000080

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
#define PTE_GLOBAL         0x100
#define PTE_RESERVED       0x080

/* Align to a power of two */
#define ALIGN_2(v, a) ((typeof(v))(((uintptr_t)(v) + (a) - 1) & ~(a - 1)))
#define PG_ALIGN(v) ALIGN_2(v, PG_SIZE)

#define PAGING_DIR_INDEX_MASK 0x3FF
#define PAGING_TABLE_INDEX_MASK 0x3FF

#define PAGING_DIR_INDEX(val) (((val) >> 22) & PAGING_DIR_INDEX_MASK)
#define PAGING_TABLE_INDEX(val) (((val) >> 12) & PAGING_TABLE_INDEX_MASK)

#define PAGING_MAKE_DIR_INDEX(val) ((val) << 22)
#define PAGING_MAKE_TABLE_INDEX(val) ((val) << 12)

#define PAGING_FRAME_MASK 0xFFFFF000
#define PAGING_ATTRS_MASK 0x00000FFF

#define PAGING_FRAME(entry) ((entry) & PAGING_FRAME_MASK)

#define __PN(addr) ((uint32_t)(addr) >> PG_SHIFT)

#ifndef ASM

#include <protura/types.h>
#include <protura/multiboot.h>

#include <protura/stddef.h>

struct page_directory_entry {
    union {
        uint32_t entry;
        struct {
            uint32_t present :1;
            uint32_t writable :1;
            uint32_t user_page :1;
            uint32_t write_through :1;
            uint32_t cache_disabled :1;
            uint32_t accessed :1;
            uint32_t zero :1;
            uint32_t page_size :1;
            uint32_t ignored :1;
            uint32_t reserved :3;
            uint32_t addr :20;
        };
    };
};

struct page_table_entry {
    union {
        uint32_t entry;
        struct {
            uint32_t present :1;
            uint32_t writable :1;
            uint32_t user_page :1;
            uint32_t write_through :1;
            uint32_t cache_disabled :1;
            uint32_t accessed :1;
            uint32_t dirty :1;
            uint32_t zero :1;
            uint32_t global :1;
            uint32_t reserved :3;
            uint32_t addr :20;
        };
    };
};

struct page_directory {
    struct page_directory_entry table[1024];
};

struct page_table {
    struct page_table_entry table[1024];
};

typedef struct page_directory pd_t;

extern struct page_directory kernel_dir;

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

void paging_setup_kernelspace(void **kbrk);

void paging_map_phys_to_virt(pa_t page_dir, va_t virt_start, pa_t phys);
void paging_map_phys_to_virt_multiple(pa_t page_dir, va_t virt, pa_t phys_start, ksize_t page_count);

pa_t paging_get_new_directory(void);

void paging_unmap_virt(va_t virt);

uintptr_t paging_get_phys(va_t virtaddr);
void paging_dump_directory(pa_t dir);

/* Low memory indicates the first 16M after the kernel is loaded This is mainly
 * for allcating pages who's physical addresses need to be known
 */
pa_t      low_get_page(void);
void      low_free_page(pa_t p);

/* High memory is everything else */
pa_t      high_get_page(void);
void      high_free_page(pa_t p);

#endif

#endif
