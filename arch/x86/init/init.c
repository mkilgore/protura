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
#include <protura/multiboot2.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/mm/memlayout.h>
#include <protura/mm/palloc.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/vm_area.h>
#include <protura/mm/kmmap.h>
#include <protura/fs/block.h>
#include <protura/fs/char.h>
#include <protura/fs/file_system.h>
#include <protura/fs/pipe.h>
#include <protura/drivers/pci.h>
#include <protura/video/fbcon.h>
#include <protura/net.h>
#include <protura/work.h>
#include <protura/cmdline.h>
#include <protura/klog.h>

#include <arch/asm.h>
#include <protura/drivers/console.h>
#include <protura/drivers/ata.h>
#include <protura/drivers/com.h>
#include <protura/video/video.h>
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

static struct fb_info framebuffer_info;

void setup_bootloader_framebuffer(void)
{
    if (!framebuffer_info.framebuffer_addr)
        return;

    kp(KP_NORMAL, "Initializing framebuffer from bootloader...\n");
    framebuffer_info.framebuffer = kmmap_pcm(framebuffer_info.framebuffer_addr, framebuffer_info.framebuffer_size, F(VM_MAP_READ) | F(VM_MAP_WRITE), PCM_WRITE_COMBINED);

    fbcon_set_framebuffer(&framebuffer_info);
    kp(KP_NORMAL, "Framebuffer from bootloader in use!\n");
}

struct sys_init arch_init_systems[] = {
    { "vm_area", vm_area_allocator_init },
    { "kwork", kwork_init },
    { "syscall", syscall_init },
    { "video", video_init },
    { "boot-fbcon", setup_bootloader_framebuffer },
    { "block-cache", block_cache_init },
    { "block-device", block_dev_init },
    { "char-device", char_dev_init },
    { "file-systems", file_systems_init },
    { "pipe", pipe_init },
#ifdef CONFIG_NET_SUPPORT
    { "net", net_init },
#endif
#ifdef CONFIG_PCI_SUPPORT
    { "pci", pci_init },
#endif
    { NULL, NULL }
};

static void handle_multiboot_info(struct multiboot_info *info, pa_t *high_addr)
{
    struct multiboot_memmap *mmap = (struct multiboot_memmap *)P2V(((struct multiboot_info*)P2V(info))->mmap_addr);

    kp(KP_NORMAL, "Using Multiboot information...\n");

    /* We haven't started paging, so MB 1 is identity mapped and we can safely
     * deref 'info' and 'cmdline' */
    /* We do this early, to avoid clobbering the cmdline */
    if (info->flags & MULTIBOOT_INFO_CMDLINE) {
        /* Make sure we don't overflow kernel_cmdline */
        info->cmdline[sizeof(kernel_cmdline) - 1] = '\0';
        strcpy(kernel_cmdline, info->cmdline);
        kp(KP_NORMAL, "Cmdline: %s\n", kernel_cmdline);
        kernel_cmdline_init();
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

        *high_addr = mmap->base_addr + mmap->length;
        *high_addr = PG_ALIGN(*high_addr) - 0x1000;
        break;
    }
}

static void handle_multiboot2_info(void *info, pa_t *high_addr)
{
    uint32_t size = *(uint32_t *)info;
    struct multiboot2_tag *tag;
    struct multiboot2_tag_basic_meminfo *meminfo;
    struct multiboot2_tag_string *str;
    struct multiboot2_tag_framebuffer_common *framebuffer;

    kp(KP_NORMAL, "Using Multiboot2 information...\n");
    kp(KP_NORMAL, "multiboot2: info size: %d\n", size);

    for (tag = (info + 8); tag->type; tag = (struct multiboot2_tag *)((char *)tag + ALIGN_2(tag->size, 8))) {
        kp(KP_NORMAL, "Multiboot tag, type: %d, size: %d\n", tag->type, tag->size);
        switch (tag->type) {
        case MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO:
            meminfo = container_of(tag, struct multiboot2_tag_basic_meminfo, tag);

            /* mem_upper starts at 1MB and is given to us in KB's */
            *high_addr = PG_ALIGN_DOWN((uint32_t)meminfo->mem_upper * 1024 + (1024 * 1024));

            kp(KP_NORMAL, "mmap: High Addr: 0x%08x\n", *high_addr);
            break;

        case MULTIBOOT2_TAG_TYPE_CMDLINE:
            str = container_of(tag, struct multiboot2_tag_string, tag);

            strncpy(kernel_cmdline, str->string, sizeof(kernel_cmdline));
            kernel_cmdline[sizeof(kernel_cmdline) - 1] = '\0';

            kp(KP_NORMAL, "Cmdline: %s\n", kernel_cmdline);
            kernel_cmdline_init();
            break;

        case MULTIBOOT2_TAG_TYPE_FRAMEBUFFER:
            framebuffer = container_of(tag, struct multiboot2_tag_framebuffer_common, tag);

            kp(KP_NORMAL, "Framebuffer, type: %d, width: %d, height: %d, BPP: %d\n",
                    framebuffer->framebuffer_type,
                    framebuffer->framebuffer_width,
                    framebuffer->framebuffer_height,
                    framebuffer->framebuffer_bpp);

            if (framebuffer->framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB)
                break;

            framebuffer_info.framebuffer_addr = framebuffer->framebuffer_addr;
            framebuffer_info.width = framebuffer->framebuffer_width;
            framebuffer_info.height = framebuffer->framebuffer_height;
            framebuffer_info.bpp = framebuffer->framebuffer_bpp;

            framebuffer_info.framebuffer_size = framebuffer_info.width * framebuffer_info.height * (framebuffer_info.bpp / 8);

            break;
        }
    }
}

/* Note: kern_start and kern_end are virtual addresses
 *
 * info is the physical address of the multiboot info structure
 */
void cmain(void *kern_start, void *kern_end, uint32_t magic, void *info)
{
    pa_t high_addr = 0;

    cpuid_init();

    cpu_init_early();

    /* initialize klog so that it captures all of the early boot logging */
    klog_init();

    /* Initalize output early for debugging */
    vt_console_early_init();
    vt_console_kp_register();

    if (com_init_early() == 0)
        com_kp_register();

    kp(KP_NORMAL, "Protura booting...\n");

    /* We setup the IDT fairly early - This is because the actual 'setup' is
     * extremely simple, and things shouldn't register interrupt handlers until
     * the IDT it setup.
     *
     * This does *not* enable interrupts. */
    idt_init();

    /* Now we parse the multiboot information. This also feeds us the
     * "high_addr" which represents the end of physical memory. This should
     * probably be fixed for something better... */
    if (magic == MULTIBOOT_BOOTLOADER_MAGIC)
        handle_multiboot_info(info, &high_addr);
    else if (magic == MULTIBOOT2_BOOTLOADER_MAGIC)
        handle_multiboot2_info(info, &high_addr);
    else
        panic("MAGIC VALUE DOES NOT MATCH MULTIBOOT OR MULTIBOOT2, CANNOT BOOT!!!!\n");

    kp(KP_NORMAL, "Memory size: %dMB\n", high_addr / 1024 / 1024);
    kp(KP_NORMAL, "Memory pages: %d\n", __PA_TO_PN(high_addr) + 1);

    /* Initalize paging as early as we can, so that we can make use of kernel
     * memory - Then start the memory manager. */
    paging_setup_kernelspace(&kern_end);

    palloc_init(&kern_end, __PA_TO_PN(high_addr) + 1);
    arch_pages_init(V2P(kern_end), high_addr);

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
    pic8259_timer_init();

    kp(KP_NORMAL, "Reading RTC time\n");
    rtc_update_time();

    kmain();
}

