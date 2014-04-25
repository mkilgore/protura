/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <drivers/term.h>
#include <protura/kmain.h>
#include <protura/multiboot.h>
#include <protura/debug.h>

#include <arch/gdt.h>
#include <arch/idt.h>
#include <arch/init.h>
#include <arch/drivers/pic8259.h>
#include <arch/drivers/pic8259_timer.h>

void cmain(void *kern_start, void *kern_end, uint32_t magic, struct multiboot_info *info)
{
/*    struct multiboot_memmap *mmap = (struct multiboot_memmap *)info->mmap_addr;
    
    while ((uint32_t)mmap < info->mmap_addr + info->mmap_length) {
        
        mmap = (struct multiboot_memmap *) ((uint32_t)mmap + mmap->size + sizeof(uint32_t));
    } */

    term_init();
    gdt_init();
    idt_init();
    pic8259_init();
    pic8259_timer_init(50);

    kprintf("Kernel Start: %p\nKernel End: %p\n", kern_start, kern_end);

    kmain();
}

