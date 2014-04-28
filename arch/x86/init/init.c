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
    /* This reports the space between the end of the kernel
     * and the end of the mapped memory as free */
    kbrk_init(kern_start, kern_end);

    term_init();
    paging_init(info);

    gdt_init();
    idt_init();
    pic8259_init();
    pic8259_timer_init(50);
    

    kprintf("Kernel Start: %p\nKernel End: %p\n", kern_start, kern_end);
    kprintf("Magic value: %x\nInfo Ptr: %p\n", magic, info);

    kprintf("Cmdline: %p\n", info->cmdline);

    kprintf("Contents: %s\n", info->cmdline);

    for (int i = 0; i < 20; i++) {
        uintptr_t p = get_page();
        kprintf("%x\t", p);
        if (i % 3 == 0)
            free_page(p);
    }

    kmain();
}

