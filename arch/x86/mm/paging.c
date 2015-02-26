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
#include <drivers/term.h>

#include <arch/asm.h>
#include <arch/idt.h>
#include <arch/kbrk.h>
#include <arch/paging.h>

struct allocator {
    pa_t phys_start;
    uint32_t pages;
    uint8_t bitmap[];
};

__align(0x1000) static struct page_directory kernel_dir = {
    { { .entry = 0 } }
};

__align(0x1000) static struct page_table kernel_pgt = {
    { { .entry = 0 } }
};

__align(0x1000) static struct page_table kernel_low_mem_pgt[KMEM_KTABLES] = {
    { { { .entry = 0 } } }
};

static struct allocator *low_mem_alloc;
static struct allocator *high_mem_alloc;

static void page_fault_handler(struct idt_frame *frame)
{
    uintptr_t p;
    asm volatile("movl %%cr2, %0": "=r" (p));
    term_setcurcolor(term_make_color(T_BLACK, T_RED));
    term_printf(" PAGE FAULT!!! ADDR: %p, ERR: %x\n", (void *)p, frame->err);
    term_printf(" PAGE DIR INDEX: %x, PAGE TABLE INDEX: %x\n", PAGING_DIR_INDEX(p) & 0x3FF, PAGING_TABLE_INDEX(p) & 0x3FF);

    while (1);
}

static void page_directory_init(va_t kern_end)
{
    pa_t cur_page, last_page = v_to_p(PG_ALIGN(kern_end)) >> 12, cur_table;
    int table;
    kernel_dir.table[KMEM_KPAGE].entry = v_to_p(&kernel_pgt) | PDE_PRESENT | PDE_WRITABLE;

    for (table = 0; table < KMEM_KTABLES; table++)
        kernel_dir.table[KMEM_KPAGE + table + 1].entry = v_to_p(kernel_low_mem_pgt + table) | PDE_PRESENT | PDE_WRITABLE;

    for (cur_page = 0; cur_page < last_page; cur_page++)
        kernel_pgt.table[cur_page].entry = PAGING_MAKE_TABLE_INDEX(cur_page) | PTE_PRESENT | PTE_WRITABLE;

    for (cur_table = 0; cur_table < KMEM_KTABLES; cur_table++) {
        for (cur_page = 0; cur_page < 1024; cur_page++) {
            pa_t addr = ((PAGING_MAKE_DIR_INDEX(cur_table + 1)) + PAGING_MAKE_TABLE_INDEX(cur_page)) + last_page;
            kernel_low_mem_pgt[cur_table].table[cur_page].entry = addr | PTE_PRESENT | PTE_WRITABLE;
        }
    }

    set_current_page_directory(v_to_p(&kernel_dir));
}

void paging_init(va_t kernel_end, pa_t last_physical_address)
{
    va_t k_end, k_start;
    size_t low_size, high_size;

    low_size = ((KMEM_LOW_SIZE >> 12) >> 3) + 1;
    high_size = (((last_physical_address - v_to_p(kernel_end)) >> 12) >> 3) + 1;

    low_mem_alloc = kbrk(sizeof(struct allocator) + low_size, alignof(struct allocator));
    high_mem_alloc = kbrk(sizeof(struct allocator) + high_size, alignof(struct allocator));

    get_kernel_addrs(&k_start, &k_end);

    k_start = PG_ALIGN(k_start);
    k_end = ALIGN(k_end, 0x00400000);

    low_mem_alloc->pages = KMEM_LOW_SIZE >> 12;
    high_mem_alloc->pages = (last_physical_address - v_to_p(k_end) - KMEM_LOW_SIZE) >> 12;

    low_mem_alloc->phys_start = v_to_p(k_end);
    high_mem_alloc->phys_start = v_to_p(k_end) + KMEM_LOW_SIZE;

    memset(low_mem_alloc->bitmap, 0, low_size);
    memset(high_mem_alloc->bitmap, 0, high_size);

    irq_register_callback(14, page_fault_handler);

    page_directory_init(k_end);

    return ;
}

void paging_dump_directory(void)
{
    uintptr_t page_dir;
    struct page_directory *cur_dir;
    int32_t table_off, page_off;
    struct page_table *cur_page_table;

    page_dir = get_current_page_directory();
    cur_dir = P2V(page_dir);

    for (table_off = 0; table_off != 1024; table_off++) {
        kprintf("Dir %d: %x\n", table_off, cur_dir->table[table_off].entry);
        if (cur_dir->table[table_off].entry & PDE_PRESENT) {
            cur_page_table = (struct page_table *)P2V(cur_dir->table[table_off].entry & ~0x3FF);
            for (page_off = 0; page_off != 1024; page_off++)
                kprintf("  Page: %d: %x\n", page_off, cur_page_table->table[page_off].entry);
        }
    }
}

void paging_map_phys_to_virt(va_t virt, pa_t phys)
{
    uintptr_t page_dir;
    struct page_directory *cur_dir;
    int32_t table_off, page_off;
    struct page_table *cur_page_table;
    uintptr_t virt_start = (uintptr_t)(virt);

    page_dir = get_current_page_directory();
    cur_dir = P2V(page_dir);

    table_off = PAGING_DIR_INDEX(virt_start);
    page_off = PAGING_TABLE_INDEX(virt_start);

    if (!(cur_dir->table[table_off].entry & PDE_PRESENT)) {
        uintptr_t new_page = low_get_page();
        memset((void *)P2V(new_page), 0, sizeof(struct page_table));

        cur_dir->table[table_off].entry = new_page | PDE_PRESENT | PDE_WRITABLE;
    }

    cur_page_table = (struct page_table *)P2V(PAGING_FRAME(cur_dir->table[table_off].entry));

    cur_page_table->table[page_off].entry = PAGING_FRAME(phys) | PTE_PRESENT | PTE_WRITABLE;
}

void paging_map_phys_to_virt_multiple(va_t virt, pa_t phys_start, size_t page_count)
{
    uintptr_t virt_ptr = (uintptr_t)virt;
    for (; page_count != 0; page_count--, virt_ptr += 0x1000, phys_start += 0x1000)
        paging_map_phys_to_virt((va_t)(virt_ptr), phys_start);

    return ;
}

uintptr_t paging_get_phys(va_t virt)
{
    struct page_directory *cur_dir;
    struct page_table *cur_page_table;
    int32_t table_off, page_off;
    uintptr_t virtaddr = (uintptr_t)virt;

    table_off = PAGING_DIR_INDEX(virtaddr);
    page_off = PAGING_TABLE_INDEX(virtaddr);

    cur_dir = P2V(get_current_page_directory());

    if (!(cur_dir->table[table_off].entry & PDE_PRESENT))
        return 0;

    cur_page_table = (struct page_table *)P2V(cur_dir->table[table_off].entry & 0xFFFFF000);

    if (!(cur_page_table->table[page_off].entry & PDE_PRESENT))
        return 0;

    return cur_page_table->table[page_off].entry & 0xFFFFF000;
}

static uintptr_t get_page(struct allocator *alloc)
{
    uint8_t mask;
    uint32_t cur_byte, max = alloc->pages >> 3;
    uint32_t page_n = 0;

    for (cur_byte = 0; cur_byte < max; cur_byte++) {
        for (mask = 0; mask != 8; mask++) {
            if ((alloc->bitmap[cur_byte] & (1 << mask)) == 0) {
                page_n = (cur_byte << 3) + mask;
                alloc->bitmap[cur_byte] |= (1 << mask);

                goto eloop;
            }
        }
    }

eloop:

    return alloc->phys_start + (page_n << 12);
}

static void free_page(struct allocator *alloc, uintptr_t p)
{
    uint8_t mask;
    uint32_t byte;
    uintptr_t offset;

    if (alloc->phys_start > p || (alloc->phys_start + (alloc->pages << 12)) < p)
        return;

    offset = p - alloc->phys_start;
    byte = offset >> 15;
    mask = (offset >> 12) & 0x7;

    alloc->bitmap[byte] &= ~(1 << mask);
}

uintptr_t low_get_page(void)
{
    return get_page(low_mem_alloc);
}

void low_free_page(uintptr_t p)
{
    free_page(low_mem_alloc, p);
}

uintptr_t high_get_page(void)
{
    return get_page(high_mem_alloc);
}

void high_free_page(uintptr_t p)
{
    free_page(high_mem_alloc, p);
}

