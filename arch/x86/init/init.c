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
#include <protura/string.h>
#include <mm/memlayout.h>
#include <mm/kmalloc.h>
#include <fs/vfsnode.h>

#include <arch/asm.h>
#include <drivers/term.h>
#include <arch/gdt.h>
#include <arch/idt.h>
#include <arch/init.h>
#include <arch/drivers/com1_debug.h>
#include <arch/drivers/pic8259.h>
#include <arch/drivers/pic8259_timer.h>
#include <arch/drivers/keyboard.h>
#include <arch/alloc.h>
#include <arch/bootmem.h>
#include <arch/paging.h>
#include <arch/pmalloc.h>
#include <arch/task.h>
#include <arch/cpu.h>
#include <arch/syscall.h>

char kernel_cmdline[2048];

struct sys_init arch_init_systems[] = {
    { "pic8259_timer", pic8259_timer_init },
    { "keyboard", keyboard_init },
    { "syscall", syscall_init },

    { NULL, NULL }
};

/* Note: kern_start and kern_end are virtual addresses
 *
 * info is the physical address of the multiboot info structure
 */
void cmain(void *kern_start, void *kern_end, uint32_t magic, struct multiboot_info *info)
{
    struct multiboot_memmap *mmap = (struct multiboot_memmap *)P2V(((struct multiboot_info*)P2V(info))->mmap_addr);
    uintptr_t high_addr = 0;


    /* Initalize output early for debugging */
    com1_init();
    term_init();

    kprintf("Protura booting...\n");

    /* We haven't started paging, so MB 1 is identity mapped and we can safely
     * deref 'info' and 'cmdline' */
    /* We do this early, to avoid clobbering the cmdline */
    if (info->flags & MULTIBOOT_INFO_CMDLINE) {
        /* Make sure we don't overflow kernel_cmdline */
        info->cmdline[sizeof(kernel_cmdline) - 1] = '\0';
        strcpy(kernel_cmdline, info->cmdline);
        kprintf("Cmdline: %s\n", kernel_cmdline);
    }

    kprintf("mmap: %p\n", mmap);

    for (; V2P(mmap) < info->mmap_addr + info->mmap_length
         ; mmap = (struct multiboot_memmap *) ((uint32_t)mmap + mmap->size + sizeof(uint32_t))) {
        kprintf("mmap: %llx to %llx, type: %d\n", mmap->base_addr,
                mmap->base_addr + mmap->length, mmap->type);

        /* A type of non-one means it's not usable memory - just ignore it */
        if (mmap->type != 1)
            continue;

        /* A small hack - We skip the map for the first MB since we map it separate.
         * The first MB isn't usable conventional memory anyway because of the
         * stuff mapped inside it. */
        if (mmap->base_addr == 0)
            continue;

        high_addr = mmap->base_addr + mmap->length;
        high_addr = PG_ALIGN(high_addr) - 0x1000;
        break;
    }

    kprintf("Memory size: %dMB\n", high_addr / 1024 / 1024);

    /* This call sets the space between the end of the kernel and the end of
     * the mapped memory as 'free' so we can do some crude allocating of memory.
     * This is literally only used to allocate some memory to get our
     * page-allocator up and going. Once kmalloc_init() is called, kbrk can't be
     * used and kmalloc should be used instead for allocations. */
    bootmem_init(V2P(kern_end));

    /* Initalize paging as early as we can */
    paging_init(kern_end, high_addr);

    pmalloc_init(kern_end, high_addr);

    bootmem_transfer_to_pmalloc();


    /* Initalize our actual allocator - This also disables kbrk from every
     * being used again, since doing-so would over-write memory allocated via
     * kmalloc. */
    kmalloc_init();

    /* Load the initial GDT and IDT setups */
    cpu_info_init();
    idt_init();

    kprintf("Kernel Start: %p\nKernel End: %p\n", kern_start, kern_end);

    /* Initalize the 8259 PIC - This has to be initalized before we can enable interrupts on the CPU */
    kprintf("Initalizing the 8259 PIC\n");
    pic8259_init();

    kmain();
}

