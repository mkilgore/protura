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
#include <protura/snprintf.h>
#include <protura/debug.h>
#include <protura/mm/memlayout.h>
#include <protura/drivers/term.h>
#include <protura/mm/palloc.h>
#include <protura/mm/vm.h>
#include <protura/mm/vm_area.h>
#include <protura/task.h>
#include <protura/fs/fs.h>
#include <protura/fs/sys.h>
#include <protura/drivers/tty.h>

#include <arch/asm.h>
#include <arch/cpu.h>
#include <arch/idt.h>
#include <arch/cpuid.h>
#include <arch/task.h>
#include <arch/paging.h>
#include <arch/backtrace.h>

__align(0x1000) struct page_directory kernel_dir = {
    { { .entry = 0 } }
};

#define PG_ERR_FLAG_TYPE 0
#define PG_ERR_FLAG_ACCESS_TYPE 1
#define PG_ERR_FLAG_PRIV 2
#define PG_ERR_FLAG_RESERVE_BITS 3

#define PG_ERR_TYPE_NON_PRESENT 0x00
#define PG_ERR_TYPE_PROTECTION  0x01

#define PG_ERR_ACCESS_TYPE_WRITE 0x02
#define PG_ERR_ACCESS_TYPE_READ  0x00

#define PG_ERR_PRIV_USER   0x04
#define PG_ERR_PRIV_KERNEL 0x00

#define PG_ERR_RESERVE_BITS_WRONG 0x08
#define PG_ERR_RESERVE_BITS_RIGHT 0x00

#define pg_err_non_present_page(err) (((err) & F(PG_ERR_FLAG_TYPE)) == PG_ERR_TYPE_NON_PRESENT)
#define pg_err_protection_fault(err) (((err) & F(PG_ERR_FLAG_TYPE)) == PG_ERR_TYPE_PROTECTION)

#define pg_err_was_user(err) (((err) & F(PG_ERR_FLAG_PRIV)) == PG_ERR_PRIV_USER)
#define pg_err_was_kernel(err) (((err) & F(PG_ERR_FLAG_PRIV)) == PG_ERR_PRIV_KERNEL)

#define pg_err_was_read(err) (((err) & F(PG_ERR_FLAG_ACCESS_TYPE)) == PG_ERR_ACCESS_TYPE_READ)
#define pg_err_was_write(err) (((err) & F(PG_ERR_FLAG_ACCESS_TYPE)) == PG_ERR_ACCESS_TYPE_WRITE)

static void paging_dump_stack(struct irq_frame *frame, uintptr_t p)
{
    //term_setcurcolor(term_make_color(T_BLACK, T_RED));
    kp(KP_ERROR, "PAGE FAULT!!! AT: %p, ADDR: %p, ERR: 0x%08x\n", (void *)frame->eip, (void *)p, frame->err);

    kp(KP_ERROR, "TYPE: %s, ACCESS: %s, PRIV: %s\n",
            pg_err_non_present_page(frame->err)? "non-present page": "protection fault",
            pg_err_was_read(frame->err)? "read": "write",
            pg_err_was_user(frame->err)? "user": "kernel"
        );

    kp(KP_ERROR, "PAGE DIR INDEX: 0x%08x, PAGE TABLE INDEX: 0x%08x\n", PAGING_DIR_INDEX(p) & 0x3FF, PAGING_TABLE_INDEX(p) & 0x3FF);
    struct task *current = cpu_get_local()->current;

    kp(KP_ERROR, "EAX: 0x%08x EBX: 0x%08x ECX: 0x%08x EDX: 0x%08x\n",
        frame->eax,
        frame->ebx,
        frame->ecx,
        frame->edx);

    kp(KP_ERROR, "ESI: 0x%08x EDI: 0x%08x ESP: 0x%08x EBP: 0x%08x\n",
        frame->esi,
        frame->edi,
        frame->esp,
        frame->ebp);

    //term_setcurcolor(term_make_color(T_WHITE, T_BLACK));
    kp(KP_ERROR, "Stack backtrace:\n");
    dump_stack_ptr((void *)frame->ebp);
    if (current && !flag_test(&current->flags, TASK_FLAG_KERNEL)) {
        kp(KP_ERROR, "Current running program: %s\n", current->name);

        if (current->context.frame) {
            kp(KP_ERROR, "EAX: 0x%08x EBX: 0x%08x ECX: 0x%08x EDX: 0x%08x\n",
                    current->context.frame->eax,
                    current->context.frame->ebx,
                    current->context.frame->ecx,
                    current->context.frame->edx);

            kp(KP_ERROR, "ESI: 0x%08x EDI: 0x%08x ESP: 0x%08x EBP: 0x%08x\n",
                    current->context.frame->esi,
                    current->context.frame->edi,
                    current->context.frame->esp,
                    current->context.frame->ebp);
            kp(KP_ERROR, "User stack dump:\n");
            dump_stack_ptr((void *)current->context.frame->ebp);
        } else {
            kp(KP_ERROR, "Context Frame: null\n");
        }
    }
    kp(KP_ERROR, "End of backtrace\n");
}

static void halt_and_dump_stack(struct irq_frame *frame, uintptr_t p)
{
    paging_dump_stack(frame, p);
    kp(KP_ERROR, "Kernel halting\n");

    while (1)
        hlt();
}

#define SEG_FAULT_MSG "Seg-fault - Program terminated\n"

static void page_fault_handler(struct irq_frame *frame, void *param)
{
    uintptr_t p;

    /* cr2 contains the address that we faulted on */
    asm ("movl %%cr2, %0": "=r" (p));

    struct task *current = cpu_get_local()->current;

    /* Check if this page was a fault we can handle */
    int ret = address_space_handle_pagefault(current->addrspc, (va_t)p);
    if (!ret)
        return;

    paging_dump_stack(frame, p);

    if (pg_err_was_kernel(frame->err))
        halt_and_dump_stack(frame, p);

    /* Program seg-faulted and we couldn't handle it - Attempt to display
     * message and exit. */
    if (current->tty) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Seg-Fault - %d terminated\n", current->pid);

        tty_write_buf(current->tty, buf, strlen(buf));
    }

    flag_set(&current->flags, TASK_FLAG_KILLED);
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
    va_t kmap_start = (va_t)CONFIG_KERNEL_KMAP_START;
    int kmap_dir_start = PAGING_DIR_INDEX(kmap_start);

    kp(KP_NORMAL, "kmap_end: %p, dir: 0x%03x\n", kmap_start, kmap_dir_start);

    /* We start at the last table from low-memory */
    /* We use 4MB pages if we're able to, since they're nicer and we're never
     * going to be editing this map again. */
    if (!cpuid_has_pse()) {
        *kbrk = PG_ALIGN(*kbrk);
        for (cur_table = table_start; cur_table < kmap_dir_start; cur_table++) {
            new_page = V2P(*kbrk);
            page_tbl = *kbrk;

            *kbrk += PG_SIZE;

            for (cur_page = 0; cur_page < 1024; cur_page++)
                page_tbl->entries[cur_page].entry = (PAGING_MAKE_DIR_INDEX(cur_table - table_start)
                                                    + PAGING_MAKE_TABLE_INDEX(cur_page))
                                                    | PTE_PRESENT | PTE_WRITABLE | tbl_gbl_bit;

            page_dir->entries[cur_table].entry = new_page | PDE_PRESENT | PDE_WRITABLE;
        }
    } else {
        for (cur_table = table_start; cur_table < kmap_dir_start; cur_table++)
            page_dir->entries[cur_table].entry = PAGING_MAKE_DIR_INDEX(cur_table - table_start)
                                                 | PDE_PRESENT | PDE_WRITABLE | PDE_PAGE_SIZE | dir_gbl_bit;
    }

    /* Setup directories for kmap */
    for (cur_table = kmap_dir_start; cur_table < 0x400; cur_table++) {
        new_page = V2P(*kbrk);
        page_tbl = *kbrk;

        *kbrk += PG_SIZE;

        memset(page_tbl, 0, sizeof(PG_SIZE));

        page_dir->entries[cur_table].entry = new_page | PDE_PRESENT | PDE_WRITABLE | dir_gbl_bit;
    }
}

void paging_setup_kernelspace(void **kbrk)
{
    uint32_t pse, pge;

    /* We make use of both PSE and PGE if the CPU supports them. */
    pse = (cpuid_has_pse())? CR4_PSE: 0;
    pge = (cpuid_has_pge())? CR4_GLOBAL: 0;

    if (pse || pge) {
        uint32_t cr4= cpu_get_cr4();
        cr4 |= pse | pge;
        cpu_set_cr4(cr4);
    }

    kp(KP_NORMAL, "Setting-up initial kernel page-directory\n");
    setup_kernel_pagedir(kbrk);

    set_current_page_directory(v_to_p(&kernel_dir));

    irq_register_callback(14, page_fault_handler, "Page fault handler", IRQ_SYSCALL, NULL);

    return ;
}

void paging_dump_directory(pa_t page_dir)
{
    struct page_directory *cur_dir;
    int32_t table_off, page_off;
    struct page_table *cur_page_table;

    cur_dir = P2V(page_dir);

    for (table_off = 0; table_off != 1024; table_off++) {
        if (cur_dir->entries[table_off].entry & PDE_PRESENT) {
            kp(KP_NORMAL, "Dir %d: %x\n", table_off, cur_dir->entries[table_off].entry);
            cur_page_table = (struct page_table *)P2V(cur_dir->entries[table_off].entry & ~0x3FF);
            for (page_off = 0; page_off != 1024; page_off++)
                if (cur_page_table->entries[page_off].entry & PTE_PRESENT)
                    kp(KP_NORMAL, "  Page: %d: %x\n", page_off, cur_page_table->entries[page_off].entry);
        }
    }
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

    if (!(cur_dir->entries[table_off].entry & PDE_PRESENT))
        return 0;

    cur_page_table = (struct page_table *)P2V(cur_dir->entries[table_off].entry & 0xFFFFF000);

    if (!(cur_page_table->entries[page_off].entry & PDE_PRESENT))
        return 0;

    return cur_page_table->entries[page_off].entry & 0xFFFFF000;
}

void vm_area_map(va_t va, pa_t address, flags_t vm_flags)
{
    pgd_t *dir;
    pgt_t *table;
    uint32_t table_off, page_off;
    uintptr_t table_entry;

    table_entry = address | PTE_PRESENT;

    if (flag_test(&vm_flags, VM_MAP_WRITE))
        table_entry |= PTE_WRITABLE;

    if (flag_test(&vm_flags, VM_MAP_NOCACHE))
        table_entry |= PTE_CACHE_DISABLE;

    if (flag_test(&vm_flags, VM_MAP_WRITETHROUGH))
        table_entry |= PTE_WRITE_THROUGH;

    table_off = PAGING_DIR_INDEX(va);
    page_off = PAGING_TABLE_INDEX(va);

    dir = P2V(get_current_page_directory());

    table = P2V(PAGING_FRAME(dir->entries[table_off].entry));

    table->entries[page_off].entry = table_entry;
}

void vm_area_unmap(va_t va)
{
    pgd_t *dir;
    pgt_t *table;
    uint32_t table_off, page_off;

    table_off = PAGING_DIR_INDEX(va);
    page_off = PAGING_TABLE_INDEX(va);

    dir = P2V(get_current_page_directory());
    table = P2V(PAGING_FRAME(dir->entries[table_off].entry));

    table->entries[page_off].entry = 0;

    flush_tlb_single(PG_ALIGN_DOWN(va));
}

