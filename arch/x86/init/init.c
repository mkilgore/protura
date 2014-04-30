/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/kmain.h>
#include <protura/multiboot.h>
#include <protura/debug.h>
#include <mm/memlayout.h>

#include <drivers/term.h>
#include <arch/gdt.h>
#include <arch/idt.h>
#include <arch/init.h>
#include <arch/drivers/pic8259.h>
#include <arch/drivers/pic8259_timer.h>
#include <arch/kbrk.h>
#include <arch/paging.h>


/* Note: kern_start and kern_end are virtual addresses
 *
 * info is the physical address of the multiboot info structure
 */

void cmain(void *kern_start, void *kern_end, uint32_t magic, struct multiboot_info *info)
{
    struct multiboot_memmap *mmap = (struct multiboot_memmap *)P2V(((struct multiboot_info*)P2V(info))->mmap_addr);
    uintptr_t high_addr = 0;

    /* This reports the space between the end of the kernel
     * and the end of the mapped memory as free */
    kbrk_init(kern_start, kern_end);


    for (; (uint32_t)mmap < info->mmap_addr + info->mmap_length
         ; mmap = (struct multiboot_memmap *) ((uint32_t)mmap + mmap->size + sizeof(uint32_t))) {
        kprintf("mmap: %llx to %llx, type: %d\n", mmap->base_addr,
                mmap->base_addr + mmap->length, mmap->type);

        /* A type of non-one means it's not usable memory - just ignore it */
        if (mmap->type != 1)
            continue;

        /* A small hack - We skip the map for the first MB since we map it separate */
        if (mmap->base_addr == 0)
            continue;

        high_addr = mmap->base_addr + mmap->length;
        high_addr = PG_ALIGN(high_addr) - 0x1000;
        break;
    }


    term_init();
    paging_init((uintptr_t)kern_end, high_addr);

    gdt_init();
    idt_init();
    pic8259_init();
    pic8259_timer_init(50);

    kprintf("Kernel Start: %p\nKernel End: %p\n", kern_start, kern_end);
    kprintf("Magic value: %x\nInfo Ptr: %p\n", magic, info);

    kprintf("Cmdline: %p\n", P2V(((struct multiboot_info *)P2V(info))->cmdline));

    kprintf("Contents: %s\n", (char *)P2V(((struct multiboot_info *)P2V(info))->cmdline));

    for (int i = 0; i < 20; i++) {
        uintptr_t p = low_get_page();
        kprintf("%x %x\t", p, P2V(p));
        *(int *) (P2V(p)) = 2;
        kprintf("Test: %d\n", *(int *)(P2V(p)));
        if (i % 3 == 0)
            low_free_page(p);
    }

    kmain();
}

