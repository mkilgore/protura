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
#include <protura/kassert.h>
#include <protura/wait.h>

#include <arch/spinlock.h>
#include <arch/idt.h>
#include <arch/drivers/pic8259.h>
#include <arch/asm.h>
#include <protura/fs/block.h>
#include <protura/drivers/ide.h>


enum {
    IDE_IRQ = 14,

    IDE_PORT_BASE = 0x1F0,

    IDE_PORT_DATA = IDE_PORT_BASE + 0,
    IDE_PORT_FEAT_ERR = IDE_PORT_BASE + 1,
    IDE_PORT_SECTOR_CNT = IDE_PORT_BASE + 2,
    IDE_PORT_LBA_LOW_8 = IDE_PORT_BASE + 3,
    IDE_PORT_LBA_MID_8= IDE_PORT_BASE + 4,
    IDE_PORT_LBA_HIGH_8 = IDE_PORT_BASE + 5,
    IDE_PORT_DRIVE_HEAD = IDE_PORT_BASE + 6,
    IDE_PORT_COMMAND_STATUS = IDE_PORT_BASE + 7,

    IDE_PORT_PRIMARY_CTL = 0x3F6,
};

enum ide_status_format {
    IDE_STATUS_BUSY = (1 << 7),
    IDE_STATUS_READY = (1 << 6),
    IDE_STATUS_DRIVE_FAULT = (1 << 5),
    IDE_STATUS_DATA_READ = (1 << 3),
    IDE_STATUS_ERROR = (1 << 0),
};

enum ide_ctl_format {
    IDE_CTL_STOP_INT = (1 << 1),
    IDE_CTL_RESET = (1 << 2)
};

/* Lower 4 bits of the drive_head hold the extra 4-bit for LBA28 mode */
enum ide_drive_head_format {
    IDE_DH_SHOULD_BE_SET = (1 << 5) | (1 << 7),
    IDE_DH_LBA = (1 << 6),

    /* Set this bit to use the slave IDE drive instead of master */
    IDE_DH_SLAVE = (1 << 4),
};

enum ide_command_format {
    IDE_COMMAND_PIO_LBA28_READ = 0x20,
    IDE_COMMAND_PIO_LBA28_WRITE = 0x30,
    IDE_COMMAND_CACHE_FLUSH = 0xE7,
};

enum {
    IDE_SECTOR_SIZE = 512,
};

struct ide_state {
    struct spinlock lock;

    list_head_t block_queue;
};

static struct ide_state ide_state = {
    .lock = SPINLOCK_INIT("ide_state lock"),
    .block_queue = LIST_HEAD_INIT(ide_state.block_queue)
};

static int __ide_wait_ready(void)
{
    int ret;

    do {
        ret = inb(IDE_PORT_COMMAND_STATUS);
    } while ((ret & (IDE_STATUS_BUSY | IDE_STATUS_READY)) != IDE_STATUS_READY);

    return ret;
}

static void __ide_start_queue(void)
{
    struct block *b;
    sector_t disk_sector;
    int sector_count;

    /* We don't actually remove this block from the queue here. That will be
     * done in the interrupt handler once we're completely done with this block
     */
    b = list_first(&ide_state.block_queue, struct block, block_list_node);

    sector_count = b->block_size / IDE_SECTOR_SIZE;
    disk_sector = b->sector * sector_count;
    /* Start the IDE data transfer */

    __ide_wait_ready();

    kp(KP_TRACE, "IDE: sector=%d\n", disk_sector);

    outb(IDE_PORT_SECTOR_CNT, sector_count);
    outb(IDE_PORT_LBA_LOW_8, (disk_sector) & 0xFF);
    outb(IDE_PORT_LBA_MID_8, (disk_sector >> 8) & 0xFF);
    outb(IDE_PORT_LBA_HIGH_8, (disk_sector >> 16) & 0xFF);
    outb(IDE_PORT_DRIVE_HEAD, IDE_DH_SHOULD_BE_SET
                                | IDE_DH_LBA
                                | ((disk_sector >> 24) & 0x0F)
                                );

    if (b->dirty) {
        int i;

        outb(IDE_PORT_COMMAND_STATUS, IDE_COMMAND_PIO_LBA28_WRITE);

        /* we have to send every sector individually */
        for (i = 0; i < sector_count; i++) {
            if ((__ide_wait_ready() & (IDE_STATUS_ERROR | IDE_STATUS_DRIVE_FAULT)) != 0)
                break;

            outsl(IDE_PORT_DATA, b->data + i * IDE_SECTOR_SIZE, IDE_SECTOR_SIZE / sizeof(uint32_t));
        }

        /* After writing, we have to flush the write cache */
        outb(IDE_PORT_COMMAND_STATUS, IDE_COMMAND_CACHE_FLUSH);
    } else {
        outb(IDE_PORT_COMMAND_STATUS, IDE_COMMAND_PIO_LBA28_READ);
    }
}

static void __ide_handle_intr(struct irq_frame *frame)
{
    struct block *b;

    if (list_empty(&ide_state.block_queue))
        return ;

    b = list_take_first(&ide_state.block_queue, struct block, block_list_node);

    /* If we were doing a read, then read the data now. We have to wait until
     * the drive is in the IDE_STATUS_READY state until we can start the read.
     *
     * Note that we don't attempt the read if there is an error reported by the
     * drive (checked via __ide_wait_ready). Also, after reading every sector
     * we have to do another __ide_wait_ready().
     */
    if (!b->dirty) {
        int i;
        int sector_count = b->block_size / IDE_SECTOR_SIZE;

        for (i = 0; i < sector_count; i++) {
            if ((__ide_wait_ready() & (IDE_STATUS_ERROR | IDE_STATUS_DRIVE_FAULT)) != 0)
                break;

            insl(IDE_PORT_DATA, b->data + i * IDE_SECTOR_SIZE, IDE_SECTOR_SIZE / sizeof(uint32_t));
        }
    }

    /* Our read/write is done, so now our block is definitely valid and not dirty */
    b->valid = 1;
    b->dirty = 0;

    /* Wakeup the owner of this block */
    scheduler_task_wake(b->owner);

    if (!list_empty(&ide_state.block_queue))
        __ide_start_queue();

    return ;
}

static void ide_handle_intr(struct irq_frame *frame)
{
    using_spinlock(&ide_state.lock)
        __ide_handle_intr(frame);
}

void ide_init(void)
{
    pic8259_enable_irq(IDE_IRQ);
    irq_register_callback(PIC8259_IRQ0 + IDE_IRQ, ide_handle_intr, "IDE", IRQ_INTERRUPT);

    outb(IDE_PORT_PRIMARY_CTL, 0);
    outb(IDE_PORT_DRIVE_HEAD, IDE_DH_SHOULD_BE_SET | IDE_DH_LBA);
}

void ide_sync_block(struct block_device *__unused dev, struct block *b)
{
    /* If the block is valid and not dirty, then there is no syncing needed */
    if (b->valid && !b->dirty)
        return ;

    int start;
    using_spinlock(&ide_state.lock) {
        start = list_empty(&ide_state.block_queue);

        list_add_tail(&ide_state.block_queue, &b->block_list_node);

        if (start)
            __ide_start_queue();
    }

    /* We sleep until the block is valid and not dirty. */
  sleep_again:
    sleep {
        if (!b->valid || b->dirty) {
            scheduler_task_yield();
            goto sleep_again;
        }
    }

    return ;
}

struct block_device_ops ide_block_device_ops = {
    .sync_block = ide_sync_block,
};

