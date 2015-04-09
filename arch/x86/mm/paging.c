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
#include <arch/alloc.h>
#include <arch/bootmem.h>
#include <arch/pmalloc.h>
#include <arch/paging.h>
#include <arch/backtrace.h>

__align(0x1000) struct page_directory kernel_dir = {
    { { .entry = 0 } }
};

static void page_fault_handler(struct idt_frame *frame)
{
    uintptr_t p;
    asm volatile("movl %%cr2, %0": "=r" (p));
    term_setcurcolor(term_make_color(T_BLACK, T_RED));
    kprintf(" PAGE FAULT!!! AT: %p, ADDR: %p, ERR: 0x%x\n", (void *)frame->eip, (void *)p, frame->err);
    kprintf(" PAGE DIR INDEX: 0x%x, PAGE TABLE INDEX: 0x%x\n", PAGING_DIR_INDEX(p) & 0x3FF, PAGING_TABLE_INDEX(p) & 0x3FF);

    term_setcurcolor(term_make_color(T_WHITE, T_BLACK));
    kprintf(" Stack backtrace:\n");
    dump_stack_ptr((void *)frame->regs.ebp);
    kprintf(" End of backtrace\n");
    kprintf(" Kernel halting\n");

    while (1)
        hlt();
}

/* To make our lives easier, we allocate and setup empty page_tables for all of
 * the kernel memory. The reason for this is that every task is going to get
 * it's own page-directory. If we setup the entries in the page-directory now,
 * then we don't have to worry about adding new ones later when we have
 * multiple page-directory's that would need to be updated with the new
 * page-directory-entry.
 */
static void setup_extra_page_tables(void)
{
    int cur_table, cur_page;
    pa_t new_page;
    struct page_directory *page_dir = &kernel_dir;
    struct page_table *page_tbl;
    int table_start = KMEM_KPAGE;

    /* We start at the last table from low-memory */
    for (cur_table = table_start; cur_table < 0x3FF; cur_table++) {
        new_page = bootmem_alloc_page();

        page_tbl = (struct page_table *)P2V(new_page);
        for (cur_page = 0; cur_page < 1024; cur_page++)
            page_tbl->table[cur_page].entry = (PAGING_MAKE_DIR_INDEX(cur_table - table_start) + PAGING_MAKE_TABLE_INDEX(cur_page)) | PTE_PRESENT | PTE_WRITABLE | PTE_GLOBAL;

        page_dir->table[cur_table].entry = new_page | PDE_PRESENT | PDE_WRITABLE | PDE_GLOBAL;
    }
}

void paging_init(va_t kernel_end, pa_t last_physical_address)
{
    irq_register_callback(14, page_fault_handler, "Page fault handler");

    kprintf("Setting-up initial kernel page-directory\n");

    setup_extra_page_tables();

    set_current_page_directory(v_to_p(&kernel_dir));

    return ;
}

void paging_dump_directory(pa_t page_dir)
{
    struct page_directory *cur_dir;
    int32_t table_off, page_off;
    struct page_table *cur_page_table;

    cur_dir = P2V(page_dir);

    for (table_off = 0; table_off != 1024; table_off++) {
        if (cur_dir->table[table_off].entry & PDE_PRESENT) {
            kprintf("Dir %d: %x\n", table_off, cur_dir->table[table_off].entry);
            cur_page_table = (struct page_table *)P2V(cur_dir->table[table_off].entry & ~0x3FF);
            for (page_off = 0; page_off != 1024; page_off++)
                if (cur_page_table->table[page_off].entry & PTE_PRESENT)
                    kprintf("  Page: %d: %x\n", page_off, cur_page_table->table[page_off].entry);
        }
    }
}

void paging_map_phys_to_virt(pa_t page_dir, va_t virt, pa_t phys)
{
    struct page_directory *cur_dir;
    int32_t table_off, page_off;
    struct page_table *cur_page_table;
    uintptr_t virt_start = (uintptr_t)(virt);

    cur_dir = P2V(page_dir);

    table_off = PAGING_DIR_INDEX(virt_start);
    page_off = PAGING_TABLE_INDEX(virt_start);

    if (!(cur_dir->table[table_off].entry & PDE_PRESENT)) {
        uintptr_t new_page = pmalloc_page_alloc(PMAL_KERNEL);
        memset((void *)P2V(new_page), 0, sizeof(struct page_table));

        cur_dir->table[table_off].entry = new_page | PDE_PRESENT | PDE_WRITABLE | PDE_USER;
    }

    cur_page_table = (struct page_table *)P2V(PAGING_FRAME(cur_dir->table[table_off].entry));

    cur_page_table->table[page_off].entry = PAGING_FRAME(phys) | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
}

void paging_map_phys_to_virt_multiple(pa_t page_dir, va_t virt, pa_t phys_start, size_t page_count)
{
    uintptr_t virt_ptr = (uintptr_t)virt;
    for (; page_count != 0; page_count--, virt_ptr += 0x1000, phys_start += 0x1000)
        paging_map_phys_to_virt(page_dir, (va_t)(virt_ptr), phys_start);

    return ;
}

void paging_unmap_virt(va_t virt)
{
    uintptr_t virt_val = (uintptr_t)virt;
    int32_t table_off, page_off;
    struct page_directory *cur_dir;
    struct page_table *cur_page_table;

    table_off = PAGING_DIR_INDEX(virt_val);
    page_off = PAGING_TABLE_INDEX(virt_val);

    cur_dir = P2V(get_current_page_directory());

    cur_page_table = (struct page_table *)P2V(PAGING_FRAME(cur_dir->table[table_off].entry));

    /* Turn off present bit */
    cur_page_table->table[page_off].entry &= ~PTE_PRESENT;

    kprintf("Cur page: %x\n", cur_page_table->table[page_off].entry);

    flush_tlb_single(virt);
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

