/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/scheduler.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/vm.h>
#include <protura/mm/kmmap.h>
#include <protura/mm/user_check.h>
#include <protura/fs/file.h>
#include <protura/list.h>

#include <protura/drivers/pci.h>
#include <protura/drivers/pci_ids.h>
#include <protura/fb.h>
#include <protura/video/video.h>
#include <protura/video/fbcon.h>
#include <protura/video/bga.h>

/* Driver for the "Bochs Graphics Adaptor" */

struct bga_display {
    struct fb_info info;

    void *mmio;
    uint16_t id;
    uint16_t virt_width;
    uint16_t virt_height;
    uint16_t x_offset;
    uint16_t y_offset;
};

static uint16_t bga_dispi_reg_read(struct bga_display *bga, int reg)
{
    if (bga->mmio) {
        return *(uint16_t *)(bga->mmio + PCI_VGA_BGA_OFFSET + reg * 2);
    } else {
        outw(VBE_DISPI_IOPORT_INDEX, reg);
        return inw(VBE_DISPI_IOPORT_DATA);
    }
}

static void bga_dispi_reg_write(struct bga_display *bga, int reg, uint16_t val)
{
    if (bga->mmio) {
        *(uint16_t *)(bga->mmio + PCI_VGA_BGA_OFFSET + reg * 2) = val;
    } else {
        outw(VBE_DISPI_IOPORT_INDEX, reg);
        outw(VBE_DISPI_IOPORT_DATA, val);
    }
}

void bga_device_init(struct pci_dev *dev)
{
    uint32_t fb_mem = 0, mmio_mem = 0;
    size_t fb_size = 0, mmio_size = 0;

    kp(KP_NORMAL, "Found BGA Display: "PRpci_dev"\n", Ppci_dev(dev));

    if (video_is_disabled()) {
        kp(KP_NORMAL, "  Video=off, Bochs display not being initialized...\n");
        return;
    }

    struct bga_display *bga = kzalloc(sizeof(*bga), PAL_KERNEL);

    if (pci_config_read_uint32(dev, PCI_REG_BAR(0)) & PCI_BAR_IO)
        goto release_bga;

    uint32_t bar = pci_config_read_uint32(dev, PCI_REG_BAR(2));
    if (bar && !(bar & PCI_BAR_IO)) {
        mmio_mem = pci_config_read_uint32(dev, PCI_REG_BAR(2)) & 0xFFFFFFF0;
        mmio_size = pci_bar_size(dev, PCI_REG_BAR(2));

        bga->mmio = kmmap(mmio_mem, mmio_size, F(VM_MAP_READ) | F(VM_MAP_WRITE));
    }

    bga->id = bga_dispi_reg_read(bga, VBE_DISPI_INDEX_ID);
    kp(KP_NORMAL, "    BGA ID: 0x%04x\n", bga->id);

    /* We don't always get the correct id, some things like qemu like to report
     * VBE_DISPI_ID0 even though they support a linear framebuffer. We just
     * continue on and hope we're good anyway. */
    if (bga->id != VBE_DISPI_ID4 && bga->id != VBE_DISPI_ID5)
        kp(KP_WARNING, "    Unsuppored BGA ID! Continuing anyway...\n");

    fb_mem = pci_config_read_uint32(dev, PCI_REG_BAR(0)) & 0xFFFFFFF0;
    fb_size = pci_bar_size(dev, PCI_REG_BAR(0));


    bga->info.framebuffer_addr = fb_mem;

    bga->info.framebuffer = kmmap_pcm(fb_mem, fb_size, F(VM_MAP_READ) | F(VM_MAP_WRITE), PCM_WRITE_COMBINED);
    bga->info.framebuffer_size = fb_size;

    bga->info.bpp = 32;
    bga->info.width = 1024;
    bga->info.height = 768;

    bga_dispi_reg_write(bga, VBE_DISPI_INDEX_ENABLE, 0);

    bga_dispi_reg_write(bga, VBE_DISPI_INDEX_BPP, bga->info.bpp);
    bga_dispi_reg_write(bga, VBE_DISPI_INDEX_XRES, bga->info.width);
    bga_dispi_reg_write(bga, VBE_DISPI_INDEX_YRES, bga->info.height);
    bga_dispi_reg_write(bga, VBE_DISPI_INDEX_VIRT_WIDTH, bga->info.width);
    bga_dispi_reg_write(bga, VBE_DISPI_INDEX_VIRT_HEIGHT, bga->info.height);
    bga_dispi_reg_write(bga, VBE_DISPI_INDEX_X_OFFSET, 0);
    bga_dispi_reg_write(bga, VBE_DISPI_INDEX_Y_OFFSET, 0);

    bga_dispi_reg_write(bga, VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

    uint32_t vmem_size = bga_dispi_reg_read(bga, VBE_DISPI_INDEX_VIDEO_MEMORY_64K) * 64 * 1024;

    bga->id = bga_dispi_reg_read(bga, VBE_DISPI_INDEX_ID);
    bga->virt_width = bga_dispi_reg_read(bga, VBE_DISPI_INDEX_VIRT_WIDTH);
    bga->virt_height = bga_dispi_reg_read(bga, VBE_DISPI_INDEX_VIRT_HEIGHT);
    bga->x_offset = bga_dispi_reg_read(bga, VBE_DISPI_INDEX_X_OFFSET);
    bga->y_offset = bga_dispi_reg_read(bga, VBE_DISPI_INDEX_Y_OFFSET);

    kp(KP_NORMAL, "    ID:        0x%04x\n", bga->id);
    kp(KP_NORMAL, "    BPP:       %d\n", bga->info.bpp);
    kp(KP_NORMAL, "    XRES:      %d\n", bga->info.width);
    kp(KP_NORMAL, "    YRES:      %d\n", bga->info.height);
    kp(KP_NORMAL, "    VIRT WID:  %d\n", bga->virt_width);
    kp(KP_NORMAL, "    VIRT HIGH: %d\n", bga->virt_height);
    kp(KP_NORMAL, "    X OFFSET:  %d\n", bga->x_offset);
    kp(KP_NORMAL, "    Y OFFSET:  %d\n", bga->y_offset);
    kp(KP_NORMAL, "    MMIO ADDR: 0x%08x\n", mmio_mem);
    kp(KP_NORMAL, "    MMIO SIZE: %d\n", mmio_size);
    kp(KP_NORMAL, "          KMMAP: %p\n", bga->mmio);
    kp(KP_NORMAL, "    FB ADDR:   0x%08x\n", fb_mem);
    kp(KP_NORMAL, "    FB SIZE:   %d\n", fb_size);
    kp(KP_NORMAL, "          KMMAP: %p\n", bga->info.framebuffer);
    kp(KP_NORMAL, "    VMEM: %d!!\n", vmem_size);

    fbcon_set_framebuffer(&bga->info);
    return;

  release_bga:
    if (bga->mmio)
        kmunmap(bga->mmio);
    kfree(bga);
    return;
}
