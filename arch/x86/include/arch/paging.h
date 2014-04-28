/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_PAGING_H
#define INCLUDE_ARCH_PAGING_H

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

#define PTE_PRESENT        0x001
#define PTE_WRITABLE       0x002
#define PTE_USER           0x004
#define PTE_WRITE_THROUGH  0x008
#define PTE_CACHE_DISABLE  0x010
#define PTE_ACCESSED       0x020
#define PTE_DIRTY          0x040
#define PTE_PAGE_SIZE      0x080
#define PTE_RESERVED       0x180

#define ALIGN(v, a) ((typeof(v))(((uintptr_t)(v) + (a) - 1) & ~(a - 1)))
#define PG_ALIGN(v) ALIGN(v, 0x1000)


#ifndef ASM

#include <protura/types.h>
#include <protura/multiboot.h>

struct page_table_ptr {
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
            uint32_t ingored :1;
            uint32_t reserved :3;
            uint32_t addr :20;
        };
    };
};

struct page_desc {
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

static __always_inline uintptr_t get_current_page_directory(void)
{
    uintptr_t pdir;
    asm volatile("movl %%cr3, %0": "=a" (pdir));
    return pdir;
}

static __always_inline void set_current_page_directory(uintptr_t page_directory)
{
    asm volatile("movl %0, %%cr3":: "r" (page_directory):"memory");
}

static __always_inline void flush_tlb_single(uintptr_t addr)
{
    asm volatile("invlpg (%0)"::"r" (addr): "memory");
}

void paging_init(struct multiboot_info *info);

void paging_map_phys_to_virt(uintptr_t virt_start, uintptr_t phys_start, size_t page_count);

uintptr_t get_page(void);
void      free_page(uintptr_t p);

#endif

#endif
