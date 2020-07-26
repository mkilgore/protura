/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/dump_mem.h>
#include <protura/string.h>
#include <protura/scheduler.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/palloc.h>
#include <protura/wait.h>

#include <arch/spinlock.h>
#include <arch/idt.h>
#include <arch/drivers/pic8259.h>
#include <arch/asm.h>
#include <protura/fs/block.h>
#include <protura/drivers/pci.h>
#include <protura/drivers/pci_ids.h>
#include <protura/fs/mbr_part.h>
#include <protura/drivers/ata.h>
#include "ata.h"

static io_t ata_reg(struct ata_drive *drive, int reg)
{
    return drive->io_base + reg;
}

static int ata_read_status(struct ata_drive *drive)
{
    return inb(ata_reg(drive, ATA_PORT_COMMAND_STATUS));
}

static int ata_read_status_delayed(struct ata_drive *drive)
{
    /* Reading the status 4 times introduces a necessary 400ns delay before
     * reading the actual status */
    ata_read_status(drive);
    ata_read_status(drive);
    ata_read_status(drive);
    ata_read_status(drive);
    return ata_read_status(drive);
}

/* Effectively, this first waits for DRQ or BUSY. If we get a DRQ we exit
 * immediately. If we get BUSY, we wait for BUSY to be cleared and return with
 * whatever status we have (error or not) */
static int ata_wait_for_drq(struct ata_drive *drive)
{
    /* FIXME: Insert actual delay logic, rather than hammering the status. But
     * this is *not* used for polling and this should be flipped relatively
     * quickly */
    int status = ata_read_status_delayed(drive);

    while (!(status & (ATA_STATUS_BUSY | ATA_STATUS_DATA_REQUEST)))
        status = ata_read_status(drive);

    if (status & ATA_STATUS_DATA_REQUEST)
        return status;

    while (status & ATA_STATUS_BUSY)
        status = ata_read_status(drive);

    return status;
}

static void ata_pio_read(struct ata_drive *drive, char *buf)
{
    insw(ata_reg(drive, ATA_PORT_DATA), buf, ATA_SECTOR_SIZE / sizeof(uint16_t));
}

static void ata_pio_write(struct ata_drive *drive, const char *buf)
{
    outsw(ata_reg(drive, ATA_PORT_DATA), buf, ATA_SECTOR_SIZE / sizeof(uint16_t));
}

static void ata_pio_write_next_sector(struct ata_drive *drive)
{
    ata_pio_write(drive, drive->current->data + drive->current_sector_offset);

    drive->sectors_left--;
    drive->current_sector_offset += ATA_SECTOR_SIZE;
}

static void ata_pio_read_next_sector(struct ata_drive *drive)
{
    ata_pio_read(drive, drive->current->data + drive->current_sector_offset);

    drive->sectors_left--;
    drive->current_sector_offset += ATA_SECTOR_SIZE;
}

static void __ata_start_request(struct ata_drive *drive)
{
    int is_slave;

    if (drive->current)
        return;

    /* Attempt to find the next block, or exit if there are none */
    if (!list_empty(&drive->block_queue_master)) {
        drive->current = list_take_first(&drive->block_queue_master, struct block, block_list_node);
        is_slave = 0;
    } else if (!list_empty(&drive->block_queue_slave)) {
        drive->current = list_take_first(&drive->block_queue_slave, struct block, block_list_node);
        is_slave = 1;
    } else {
        return;
    }

    struct block *b = drive->current;


    /* Convert from block sectors to ATA sectors */
    int sector_count = b->block_size / ATA_SECTOR_SIZE;
    sector_t sector_start = b->sector * sector_count;

    if (DEV_MINOR(b->dev)) {
        int minor = DEV_MINOR(b->dev);
        int part_no = minor - 1;

        if (part_no < b->bdev->partition_count)
            sector_start += b->bdev->partitions[part_no].start;
    }

    kp_ata("Request B: %d, sector: %d, count: %d, slave: %d\n", b->sector, sector_start, sector_count, is_slave);

    drive->current_sector_offset = 0;
    drive->sectors_left = sector_count;

    outb(ata_reg(drive, ATA_PORT_SECTOR_CNT), sector_count);
    outb(ata_reg(drive, ATA_PORT_LBA_LOW_8), sector_start & 0xFF);
    outb(ata_reg(drive, ATA_PORT_LBA_MID_8), (sector_start >> 8) & 0xFF);
    outb(ata_reg(drive, ATA_PORT_LBA_HIGH_8), (sector_start >> 16) & 0xFF);

    outb(ata_reg(drive, ATA_PORT_DRIVE_HEAD), ATA_DH_SHOULD_BE_SET
                                            | ATA_DH_LBA
                                            | ((sector_start >> 24) & 0x0F)
                                            | (is_slave? ATA_DH_SLAVE: 0)
                                            );

    if (!flag_test(&b->flags, BLOCK_DIRTY)) {
        kp_ata("Start read: %d\n", b->sector);
        outb(ata_reg(drive, ATA_PORT_COMMAND_STATUS), ATA_COMMAND_PIO_LBA28_READ);
    } else {
        kp_ata("Start write: %d\n", b->sector);
        outb(ata_reg(drive, ATA_PORT_COMMAND_STATUS), ATA_COMMAND_PIO_LBA28_WRITE);

        int status = ata_wait_for_drq(drive);

        kassert(status & ATA_STATUS_DATA_REQUEST, "Drive not ready after busy wait! Status: 0x%02x\n", status);
        kassert(!(status & ATA_STATUS_BUSY), "Drive busy after busy wait! Status: 0x%02x\n", status);

        ata_pio_write_next_sector(drive);
    }
}

static void __ata_handle_intr(struct ata_drive *drive)
{
    struct block *b = drive->current;
    int status = ata_read_status(drive);

    kp_ata("INTR, Status: 0x%02x\n", status);

    /* If the interrupt is shared, the request may not be finished yet */
    if (status & ATA_STATUS_BUSY)
        return;

    kassert(status & (ATA_STATUS_DATA_REQUEST | ATA_STATUS_READY), "Drive not busy but DRQ not set! Status: 0x%02x\n", status);
    kassert(!(status & ATA_STATUS_ERROR), "Drive reported error reading block %d! Status: 0x%02x\n", b->sector, status);

    int request_done = 0;

    if (!flag_test(&b->flags, BLOCK_DIRTY)) {
        ata_pio_read_next_sector(drive);

        if (!drive->sectors_left)
            request_done = 1;
    } else {
        if (!drive->sectors_left)
            request_done = 1;
        else
            ata_pio_write_next_sector(drive);
    }

    if (request_done) {
        block_mark_synced(b);
        block_unlockput(b);

        drive->current = NULL;

        /* Start the next request if we have one */
        __ata_start_request(drive);
    }
}

static void ata_handle_intr(struct irq_frame *frame, void *param)
{
    struct ata_drive *drive = param;

    using_spinlock(&drive->lock)
        __ata_handle_intr(drive);
}

static void ata_sync_block(struct ata_drive *drive, struct block *b, int master_or_slave)
{
    /* Shouldn't really happen, since the block is locked and they can check
     * this before calling, but check it anyway */
    if (flag_test(&b->flags, BLOCK_VALID) && !flag_test(&b->flags, BLOCK_DIRTY)) {
        block_unlock(b);
        return ;
    }

    b = block_dup(b);

    /* The block will be unlocked once syncing is done */
    using_spinlock(&drive->lock) {
        if (master_or_slave == 0)
            list_add_tail(&drive->block_queue_master, &b->block_list_node);
        else
            list_add_tail(&drive->block_queue_slave, &b->block_list_node);

        __ata_start_request(drive);
    }
}

void ata_sync_block_master(struct block_device *dev, struct block *b)
{
    return ata_sync_block(dev->priv, b, 0);
}

void ata_sync_block_slave(struct block_device *dev, struct block *b)
{
    return ata_sync_block(dev->priv, b, 1);
}

struct block_device_ops ata_master_block_device_ops = {
    .sync_block = ata_sync_block_master,
};

struct block_device_ops ata_slave_block_device_ops = {
    .sync_block = ata_sync_block_slave,
};

static void ata_identity_fix_string(uint8_t *s, size_t len)
{
    if (len % 2)
        panic("Non mod 2 length passed to ata_identity_fix_string()!\n");

    uint16_t *t = (uint16_t *)s;

    size_t i;
    for (i = 0; i < len / 2; i++)
        t[i] = (t[i] << 8) | (t[i] >> 8);

    size_t end = 0;
    for (i = 0; i < len; i++)
        if (s[i] && s[i] != ' ')
            end = i;

    for (i = end + 1; i < len; i++)
        s[i] = '\0';
}

/* NOTE: This function *requires* interrupts to be on for the timer to
 * function. This is only used during startup and polling for the identify
 * command */
static int ata_wait_for_status(struct ata_drive *drive, int status, uint32_t ms)
{
    int ret;
    uint32_t start = timer_get_ms();
    uint32_t last = start;

    do {
        ret = ata_read_status(drive);

        uint32_t cur;
        while (cur = timer_get_ms(), cur == last)
            ;

        last = cur;

        if (last - start > ms)
            return ATA_STATUS_TIMEOUT | ret;
    } while ((ret & (ATA_STATUS_BUSY | status)) != status);

    return ret;
}

static void ata_identify(struct block_device *bdev, struct ata_drive *drive)
{
    struct page *page = palloc(0, PAL_KERNEL);
    outb(ata_reg(drive, ATA_PORT_COMMAND_STATUS), ATA_COMMAND_IDENTIFY);

    int status = ata_read_status_delayed(drive);

    /* if we got zero back, the drive isn't present */
    if (status == 0)
        return;

    /* We wait for the drive to respond for one second maximum before giving up */
    status = ata_wait_for_status(drive, ATA_STATUS_READY, 1000);
    if (status & (ATA_STATUS_DRIVE_FAULT | ATA_STATUS_ERROR | ATA_STATUS_TIMEOUT) || !(status & ATA_STATUS_READY)) {
        kp(KP_NORMAL, "IDENTIFY command returned status: 0x%02x\n", status);
        goto cleanup;
    }

    ata_pio_read(drive, page->virt);

    struct ata_identify_format *id = page->virt;
    ata_identity_fix_string(id->model, sizeof(id->model));

    kp(KP_NORMAL, "IDENTIFY Model: %s, capacity: %dMB\n", id->model, id->lba_capacity * ATA_SECTOR_SIZE / 1024 / 1024);

    flag_set(&bdev->flags, BLOCK_DEV_EXISTS);
    bdev->device_size = id->lba_capacity * ATA_SECTOR_SIZE;

  cleanup:
    pfree(page, 0);
}

static void ata_identify_master(struct block_device *device, struct ata_drive *drive)
{
    outb(ata_reg(drive, ATA_PORT_DRIVE_HEAD), ATA_DH_SHOULD_BE_SET);
    ata_identify(device, drive);
}

static void ata_identify_slave(struct block_device *device, struct ata_drive *drive)
{
    outb(ata_reg(drive, ATA_PORT_DRIVE_HEAD), ATA_DH_SHOULD_BE_SET | ATA_DH_SLAVE);
    ata_identify(device, drive);
}

static int is_initialized;
static void ata_drive_init(struct ata_drive *drive)
{
    int i;
    struct block_device *master, *slave;

    /* FIXME: Hack, we only support one ATA device. The current structure can
     * easily support more, but we need some block_device changes to tie multiple storage devices under the same major. */
    if (is_initialized)
        return;

    is_initialized = 1;

    outb(drive->ctrl_io_base, ATA_CTL_RESET | ATA_CTL_STOP_INT);

    /* The reset take a few ms */
    scheduler_task_waitms(10);

    outb(drive->ctrl_io_base, ATA_CTL_STOP_INT);

    master = block_dev_get(DEV_MAKE(BLOCK_DEV_IDE_MASTER, 0));
    slave = block_dev_get(DEV_MAKE(BLOCK_DEV_IDE_SLAVE, 0));

    master->priv = drive;
    slave->priv = drive;

    kp(KP_NORMAL, "Probing ATA device: 0x%04x, 0x%04x, %d\n", drive->io_base, drive->ctrl_io_base, drive->drive_irq);
    ata_identify_master(master, drive);
    ata_identify_slave(slave, drive);

    /* We do a read status here to clear any pending interrupt before enabling them */
    ata_read_status(drive);
    outb(drive->ctrl_io_base, 0);

    int err = irq_register_callback(drive->drive_irq, ata_handle_intr, "ATA", IRQ_INTERRUPT, drive, F(IRQF_SHARED));
    if (err) {
        kp(KP_WARNING, "ATA: Interrupt %d is already in use! Check log, drive cannot be used.\n", drive->drive_irq);
        return;
    }

    if (flag_test(&master->flags, BLOCK_DEV_EXISTS)) {
        block_dev_set_block_size(DEV_MAKE(BLOCK_DEV_IDE_MASTER, 0), ATA_SECTOR_SIZE);
        int master_partition_count = mbr_add_partitions(master);

        kp(KP_NORMAL, "Master partition count: %d\n", master_partition_count);

        for (i = 0; i < master_partition_count; i++)
            master->partitions[i].block_size = ATA_SECTOR_SIZE;
    }

    if (flag_test(&slave->flags, BLOCK_DEV_EXISTS)) {
        block_dev_set_block_size(DEV_MAKE(BLOCK_DEV_IDE_SLAVE, 0), ATA_SECTOR_SIZE);
        int slave_partition_count = mbr_add_partitions(slave);

        kp(KP_NORMAL, "Slave partition count: %d\n", slave_partition_count);

        for (i = 0; i < slave_partition_count; i++)
            slave->partitions[i].block_size = ATA_SECTOR_SIZE;
    }

    if (!flag_test(&master->flags, BLOCK_DEV_EXISTS) && !flag_test(&slave->flags, BLOCK_DEV_EXISTS))
        kp(KP_NORMAL, "No ATA drives detected.\n");
}

/* FIXME: We only support one right now because there is only one ATA block device */
static struct ata_drive static_drive = {
    .lock = SPINLOCK_INIT(),
    .block_queue_master = LIST_HEAD_INIT(static_drive.block_queue_master),
    .block_queue_slave = LIST_HEAD_INIT(static_drive.block_queue_slave),
    .io_base = 0x1F0,
    .ctrl_io_base = 0x3F6,
    .drive_irq = 14,
};

void ata_pci_init(struct pci_dev *dev)
{
    /* May be zero, in that case it will get filled in later */
    io_t io_base = pci_config_read_uint32(dev, PCI_REG_BAR(0)) & 0xFFF0;
    io_t ctrl_io_base = pci_config_read_uint32(dev, PCI_REG_BAR(1)) & 0xFFF0;
    int int_line = pci_config_read_uint8(dev, PCI_REG_INTERRUPT_LINE);

    if (io_base && io_base != 1)
        static_drive.io_base = io_base;

    if (ctrl_io_base && ctrl_io_base != 1)
        static_drive.ctrl_io_base = ctrl_io_base;

    if (pci_has_interrupt_line(dev) && int_line)
        static_drive.drive_irq = int_line;

    kp(KP_NORMAL, "PCI IDE device, IO Base: 0x%04x, IO Ctrl: 0x%04x, INT: %d\n", static_drive.io_base, static_drive.ctrl_io_base, static_drive.drive_irq);

    ata_drive_init(&static_drive);
}
