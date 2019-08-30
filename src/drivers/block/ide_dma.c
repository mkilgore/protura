/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/scheduler.h>
#include <protura/wait.h>

#include <arch/memlayout.h>
#include <arch/spinlock.h>
#include <arch/idt.h>
#include <arch/drivers/pic8259.h>
#include <arch/asm.h>
#include <protura/fs/block.h>
#include <protura/drivers/pci.h>
#include <protura/drivers/pci_ids.h>
#include <protura/drivers/ide.h>
#include "ide.h"

#define KP_IDE_DMA 99

#define IDE_DMA_IO_CMD1  0
#define IDE_DMA_IO_STAT1 2
#define IDE_DMA_IO_PRDT1 4

static void ide_dma_setup_prdt(struct ide_dma_info *info, struct block *block)
{
    int i;
    info->prd_table[0].addr = (V2P(block->data));
    info->prd_table[0].bcnt = block->block_size | 0x80000000;

    for (i = 0; i < 8; i++)
        kp(KP_IDE_DMA, "PRD byte%d: 0x%02x\n", i, *((uint8_t *)info->prd_table + i));

    kp(KP_IDE_DMA, "PRD byte1: 0x%08x, byte2: 0x%08x\n", *(uint32_t *)info->prd_table, *((uint32_t *)info->prd_table + 1));

    outl(info->dma_io_base + IDE_DMA_IO_PRDT1, V2P(info->prd_table));
}

int ide_dma_setup_read(struct ide_dma_info *info, struct block *block)
{
    kp(KP_IDE_DMA, "IDE setup dma read\n");
    ide_dma_setup_prdt(info, block);

    outb(info->dma_io_base + IDE_DMA_IO_CMD1, (1 << 3)); /* r/w - Set bit 3 for read */
    outb(info->dma_io_base + IDE_DMA_IO_STAT1, inb(info->dma_io_base + IDE_DMA_IO_STAT1)); /* Per Linux, clear status register */

    return 0;
}

int ide_dma_setup_write(struct ide_dma_info *info, struct block *block)
{
    kp(KP_IDE_DMA, "IDE setup dma write\n");
    ide_dma_setup_prdt(info, block);

    outb(info->dma_io_base + IDE_DMA_IO_CMD1, (1 << 3)); /* r/w - Set bit 3 for read */
    outb(info->dma_io_base + IDE_DMA_IO_STAT1, inb(info->dma_io_base + IDE_DMA_IO_STAT1)); /* Per Linux, clear status register */

    return 0;
}

void ide_dma_start(struct ide_dma_info *info)
{
    /* Set bit 0 in the command to start the DMA request */
    kp(KP_IDE_DMA, "IDE DMA Start\n");
    outb(info->dma_io_base + IDE_DMA_IO_CMD1, inb(info->dma_io_base + IDE_DMA_IO_CMD1) | 1);

    kp(KP_IDE_DMA, "IDE stat after start: 0x%02x\n", ide_dma_check(info));
}

void ide_dma_abort(struct ide_dma_info *info)
{
    /* Force stop DMA */
    kp(KP_IDE_DMA, "IDE DMA Stop\n");
    outb(info->dma_io_base + IDE_DMA_IO_CMD1, inb(info->dma_io_base + IDE_DMA_IO_CMD1) & ~1);
}

int ide_dma_check(struct ide_dma_info *info)
{
    return inb(info->dma_io_base + IDE_DMA_IO_STAT1);
}

void ide_dma_init(struct ide_dma_info *info, struct pci_dev *dev)
{
    uint16_t command_reg;

    kp(KP_NORMAL, "Found Intel PIIX3 IDE DMA PCI device: "PRpci_dev"\n", Ppci_dev(dev));

    command_reg = pci_config_read_uint16(dev, PCI_COMMAND);

    kp(KP_NORMAL, "  PCI CMD: 0x%04x\n", command_reg);

    if (!(command_reg & PCI_COMMAND_BUS_MASTER)) {
        pci_config_write_uint16(dev, PCI_COMMAND, command_reg | PCI_COMMAND_BUS_MASTER);
        kp(KP_NORMAL, "  PCI BUS MASTERING ENABLED\n");
    }

    info->is_enabled = 1;
    info->dma_io_base = pci_config_read_uint32(dev, PCI_REG_BAR(4)) & 0xFFF0;
    info->dma_irq = pci_config_read_uint8(dev, PCI_REG_INTERRUPT_LINE);

    kp(KP_NORMAL, "  BAR4: 0x%04x\n", info->dma_io_base);
    kp(KP_NORMAL, "  Timing: 0x%08x\n", pci_config_read_uint32(dev, 0x40));

    outb(info->dma_io_base + IDE_DMA_IO_STAT1, inb(info->dma_io_base + IDE_DMA_IO_STAT1) | (0x30));
}
