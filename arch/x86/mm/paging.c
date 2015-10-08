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
#include <mm/palloc.h>

#include <arch/asm.h>
#include <arch/cpu.h>
#include <arch/idt.h>
#include <arch/cpuid.h>
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
    kprintf(" PAGE FAULT!!! AT: %p, ADDR: %p, ERR: 0x%08x\n", (void *)frame->eip, (void *)p, frame->err);
    kprintf(" PAGE DIR INDEX: 0x%08x, PAGE TABLE INDEX: 0x%08x\n", PAGING_DIR_INDEX(p) & 0x3FF, PAGING_TABLE_INDEX(p) & 0x3FF);
    kprintf(" Current running program: %s\n", (cpu_get_local()->current)? cpu_get_local()->current->name: "");

    term_setcurcolor(term_make_color(T_WHITE, T_BLACK));
    kprintf(" Stack backtrace:\n");
    dump_stack_ptr((void *)frame->ebp);
    kprintf(" End of backtrace\n");
    kprintf(" Kernel halting\n");

    while (1)
        hlt();
}

/* All of kernel-space virtual memory directly maps onto the lowest part of
 * physical memory (Or all of physical memory, if there is less physical memory
 * then the kernel's virtual memory space). This code sets up the kernel's page
 * directory to map all of the addresses in kernel space in this way. The kbrk
 * pointer is used to get pages to use as page-tables.
 *
 * Note that if we have PSE, we just use that and map all of kernel-space using
 * 4MB pages, and thus avoid ever having to allocate page-tables.
 *
 * Since every processes's page directory will have the kernel's memory mapped,
 * we can make all of these pages as global, if the processor supports it.
 */
static void setup_kernel_pagedir(void **kbrk)
{
    int cur_table, cur_page;
    pa_t new_page;
    struct page_directory *page_dir = &kernel_dir;
    struct page_table *page_tbl;
    int table_start = KMEM_KPAGE;
    int tbl_gbl_bit = (cpuid_has_pge())? PTE_GLOBAL: 0;
    int dir_gbl_bit = (cpuid_has_pge())? PDE_GLOBAL: 0;

    /* We start at the last table from low-memory */
    /* We use 4MB pages if we're able to, since they're nicer and we're never
     * going to be editing this map again. */
    if (!cpuid_has_pse()) {
        *kbrk = PG_ALIGN(*kbrk);
        for (cur_table = table_start; cur_table < 0x3FF; cur_table++) {
            new_page = V2P(*kbrk);
            page_tbl = *kbrk;

            *kbrk += PG_SIZE;

            for (cur_page = 0; cur_page < 1024; cur_page++)
                page_tbl->table[cur_page].entry = (PAGING_MAKE_DIR_INDEX(cur_table - table_start)
                                                  + PAGING_MAKE_TABLE_INDEX(cur_page))
                                                  | PTE_PRESENT | PTE_WRITABLE | tbl_gbl_bit;

            page_dir->table[cur_table].entry = new_page | PDE_PRESENT | PDE_WRITABLE;
        }
    } else {
        for (cur_table = table_start; cur_table < 0x3FF; cur_table++)
            page_dir->table[cur_table].entry = PAGING_MAKE_DIR_INDEX(cur_table - table_start)
                                               | PDE_PRESENT | PDE_WRITABLE | PDE_PAGE_SIZE | dir_gbl_bit;
    }
}

void paging_setup_kernelspace(void **kbrk)
{
    uint32_t pse, pge;

    irq_register_callback(14, page_fault_handler, "Page fault handler", IRQ_INTERRUPT);

    /* We make use of both PSE and PGE if the CPU supports them. */
    pse = (cpuid_has_pse())? CR4_PSE: 0;
    pge = (cpuid_has_pge())? CR4_GLOBAL: 0;

    if (pse || pge) {
        uint32_t cr4;

        asm volatile("movl %%cr4, %0": "=r" (cr4));
        cr4 |= pse | pge;
        asm volatile("movl %0,%%cr4": : "r" (cr4));
    }

    kprintf("Setting-up initial kernel page-directory\n");
    setup_kernel_pagedir(kbrk);

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
        pa_t new_page = palloc_phys(PAL_KERNEL);
        memset((void *)P2V(new_page), 0, sizeof(struct page_table));

        cur_dir->table[table_off].entry = new_page | PDE_PRESENT | PDE_WRITABLE | PDE_USER;
    }

    cur_page_table = (struct page_table *)P2V(PAGING_FRAME(cur_dir->table[table_off].entry));

    cur_page_table->table[page_off].entry = PAGING_FRAME(phys) | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
}

void paging_map_phys_to_virt_multiple(pa_t page_dir, va_t virt, pa_t phys_start, ksize_t page_count)
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

