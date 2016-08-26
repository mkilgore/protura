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
#include <protura/mm/memlayout.h>
#include <protura/mm/palloc.h>
#include <protura/mm/kmalloc.h>
#include <protura/fs/block.h>
#include <protura/fs/char.h>
#include <protura/fs/file_system.h>
#include <protura/fs/pipe.h>

#include <arch/asm.h>
#include <protura/drivers/term.h>
#include <protura/drivers/ide.h>
#include <protura/drivers/com.h>
#include <arch/gdt.h>
#include <arch/idt.h>
#include <arch/init.h>
#include <arch/cpuid.h>
#include <arch/drivers/pic8259.h>
#include <arch/drivers/pic8259_timer.h>
#include <arch/drivers/keyboard.h>
#include <arch/drivers/rtc.h>
#include <arch/paging.h>
#include <arch/pages.h>
#include <arch/task.h>
#include <arch/cpu.h>
#include <arch/syscall.h>

char kernel_cmdline[2048];

struct sys_init arch_init_systems[] = {
    { "pic8259_timer", pic8259_timer_init },
    { "syscall", syscall_init },
    { "block-cache", block_cache_init },
    { "block-device", block_dev_init },
    { "char-device", char_dev_init },
    { "file-systems", file_systems_init },
    { "pipe", pipe_init },
    { NULL, NULL }
};

/* Note: kern_start and kern_end are virtual addresses
 *
 * info is the physical address of the multiboot info structure
 */
void cmain(void *kern_start, void *kern_end, uint32_t magic, struct multiboot_info *info)
{
    struct multiboot_memmap *mmap = (struct multiboot_memmap *)P2V(((struct multiboot_info*)P2V(info))->mmap_addr);
    pa_t high_addr = 0;

    cpuid_init();

    /* Initalize output early for debugging */
    term_init();
    kp_output_register(term_printfv, "TERM");

    com_init_early();
    kp_output_register(com1_printfv, "COM1");

    kp(KP_NORMAL, "Protura booting...\n");

    /* We setup the IDT fairly early - This is because the actual 'setup' is
     * extremely simple, and things shouldn't register interrupt handlers until
     * the IDT it setup.
     *
     * This does *not* enable interrupts. */
    idt_init();

    /* We haven't started paging, so MB 1 is identity mapped and we can safely
     * deref 'info' and 'cmdline' */
    /* We do this early, to avoid clobbering the cmdline */
    if (info->flags & MULTIBOOT_INFO_CMDLINE) {
        /* Make sure we don't overflow kernel_cmdline */
        info->cmdline[sizeof(kernel_cmdline) - 1] = '\0';
        strcpy(kernel_cmdline, info->cmdline);
        kp(KP_NORMAL, "Cmdline: %s\n", kernel_cmdline);
    }

    kp(KP_NORMAL, "mmap: %p\n", mmap);

    for (; V2P(mmap) < info->mmap_addr + info->mmap_length
         ; mmap = (struct multiboot_memmap *) ((uint32_t)mmap + mmap->size + sizeof(uint32_t))) {
        kp(KP_NORMAL, "mmap: 0x%016llx to 0x%016llx, type: %d\n", mmap->base_addr,
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

    kp(KP_NORMAL, "Memory size: %dMB\n", high_addr / 1024 / 1024);
    kp(KP_NORMAL, "Memory pages: %d\n", __PA_TO_PN(high_addr) + 1);

    /* Initalize paging as early as we can, so that we can make use of kernel
     * memory - Then start the memory manager. */
    paging_setup_kernelspace(&kern_end);

    palloc_init(&kern_end, __PA_TO_PN(high_addr) + 1);
    arch_pages_init(V2P(kern_start), V2P(kern_end), high_addr);

    kmalloc_init();

    /* Setup the per-CPU stuff - Has to be done after kmalloc and friends are
     * setup. */
    cpu_info_init();

    kp(KP_NORMAL, "Kernel Start: %p\n", kern_start);
    kp(KP_NORMAL, "Kernel End: %p\n", kern_end);

    /* Initalize the 8259 PIC - This has to be initalized before we can enable
     * interrupts on the CPU */
    kp(KP_NORMAL, "Initalizing the 8259 PIC\n");
    pic8259_init();

    kp(KP_NORMAL, "Reading RTC time\n");
    rtc_update_time();

    kmain();
}

