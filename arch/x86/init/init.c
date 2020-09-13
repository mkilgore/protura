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
#include <protura/mm/kmmap.h>
#include <protura/drivers/pci.h>
#include <protura/video/fbcon.h>
#include <protura/work.h>
#include <protura/kparam.h>
#include <protura/klog.h>
#include <protura/mm/bootmem.h>

#include <arch/asm.h>
#include <protura/drivers/console.h>
#include <protura/drivers/com.h>
#include <protura/video/video.h>
#include <arch/gdt.h>
#include <arch/idt.h>
#include <arch/init.h>
#include <arch/cpuid.h>
#include <arch/drivers/pic8259.h>
#include <arch/drivers/pic8259_timer.h>
#include <arch/drivers/rtc.h>
#include <arch/paging.h>
#include <arch/pages.h>
#include <arch/task.h>
#include <arch/cpu.h>

char kernel_cmdline[2048];

static struct fb_info framebuffer_info;

static void setup_bootloader_framebuffer(void)
{
    if (!framebuffer_info.framebuffer_addr)
        return;

    kp(KP_NORMAL, "Initializing framebuffer from bootloader...\n");
    framebuffer_info.framebuffer = kmmap_pcm(framebuffer_info.framebuffer_addr, framebuffer_info.framebuffer_size, F(VM_MAP_READ) | F(VM_MAP_WRITE), PCM_WRITE_COMBINED);

    fbcon_set_framebuffer(&framebuffer_info);
    kp(KP_NORMAL, "Framebuffer from bootloader in use!\n");

    video_mark_disabled();
}
initcall_device(boot_fbcon, setup_bootloader_framebuffer);

/* This dependency exists because we do not know what device the GRUB
 * framebuffer is from, we we may already have a driver for it. If we load that
 * driver we will likely mess up the framebuffer. */
initcall_dependency(pci_devices, boot_fbcon);

/* The multiboot memory regions are 64-bit ints (to support PAE) and can extend
 * past the 32-bit memory space. We handle that here */
static void add_multiboot_memory_region(uint64_t start, uint64_t length)
{
    /* Skip any regions that go past the end of the 32-bit memory space */
    if (start  >= 0xFFFFFFFFLL)
        return;

    /* Clip any regions inside of the 32-bit memory space if they stretch outside it */
    if (start + length >= 0xFFFFFFFFLL)
        length  = 0xFFFFFFFFLL - start;

    bootmem_add(start, start + length);
}

static void handle_multiboot_info(struct multiboot_info *info)
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

        add_multiboot_memory_region(mmap->base_addr, mmap->length);
    }
}

static void handle_multiboot2_info(void *info)
{
    uint32_t size = *(uint32_t *)info;
    struct multiboot2_tag *tag;
    struct multiboot2_tag_basic_meminfo *meminfo;
    struct multiboot2_tag_string *str;
    struct multiboot2_tag_framebuffer_common *framebuffer;
    struct multiboot2_tag_mmap *mmap;
    struct multiboot2_mmap_entry *mmap_ent;
    int had_mmap = 0;
    pa_t basic_mem_high_mem = 0;

    kp(KP_NORMAL, "Using Multiboot2 information...\n");
    kp(KP_NORMAL, "multiboot2: info size: %d\n", size);

    for (tag = (info + 8); tag->type; tag = (struct multiboot2_tag *)((char *)tag + ALIGN_2(tag->size, 8))) {
        kp(KP_NORMAL, "Multiboot tag, type: %d, size: %d\n", tag->type, tag->size);
        switch (tag->type) {
        case MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO:
            meminfo = container_of(tag, struct multiboot2_tag_basic_meminfo, tag);

            /* mem_upper starts at 1MB and is given to us in KB's */
            basic_mem_high_mem  = PG_ALIGN_DOWN((uint32_t)meminfo->mem_upper * 1024 + (1024 * 1024));
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

        case MULTIBOOT2_TAG_TYPE_MMAP:
            mmap = container_of(tag, struct multiboot2_tag_mmap, tag);
            had_mmap = 1;

            for (mmap_ent = mmap->entries;
                    (uint8_t *)mmap_ent < (uint8_t *)tag + tag->size;
                    mmap_ent = (struct multiboot2_mmap_entry *)((uint8_t *)mmap_ent + mmap->entry_size)) {

                kp(KP_NORMAL, "mmap: 0x%016llx to 0x%016llx, type: %d\n", mmap_ent->addr,
                        mmap_ent->addr + mmap_ent->len, mmap_ent->type);

                if (mmap_ent->type != MULTIBOOT_MEMORY_AVAILABLE)
                    continue;

                add_multiboot_memory_region(mmap_ent->addr, mmap_ent->len);
            }

            break;
        }
    }

    if (!had_mmap) {
        kp(KP_NORMAL, "mmap: High Addr: 0x%08x\n", basic_mem_high_mem);

        bootmem_add(1024 * 1024 * 1024, basic_mem_high_mem);
    }
}

extern char kern_start, kern_end;

/* info is the physical address of the multiboot info structure. An identity
 * mapping is currently setup that makes it valid though. */
void cmain(uint32_t magic, void *info)
{
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
    kp(KP_NORMAL, "Kernel Physical Memory Location: 0x%08x-0x%08x\n", V2P(&kern_start), V2P(&kern_end));

    /* We setup the IDT fairly early - This is because the actual 'setup' is
     * extremely simple, and things shouldn't register interrupt handlers until
     * the IDT it setup.
     *
     * This does *not* enable interrupts. */
    idt_init();

    if (magic == MULTIBOOT_BOOTLOADER_MAGIC)
        handle_multiboot_info(info);
    else if (magic == MULTIBOOT2_BOOTLOADER_MAGIC)
        handle_multiboot2_info(info);
    else
        panic("MAGIC VALUE DOES NOT MATCH MULTIBOOT OR MULTIBOOT2, CANNOT BOOT!!!!\n");

    /* Initalize paging as early as we can, so that we can make use of kernel
     * memory - Then start the memory manager. */
    paging_setup_kernelspace();

    bootmem_setup_palloc();

    kmalloc_init();

    /* Setup the per-CPU stuff - Has to be done after kmalloc and friends are
     * setup. */
    cpu_info_init();

    /* Initalize the 8259 PIC - This has to be initalized before we can enable
     * interrupts on the CPU */
    kp(KP_NORMAL, "Initalizing the 8259 PIC\n");
    pic8259_init();
    pic8259_timer_init();

    kp(KP_NORMAL, "Reading RTC time\n");
    rtc_update_time();

    kmain();
}
