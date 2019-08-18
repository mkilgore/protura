/*
 * Copyright (C) 2015 Matt Kilgore
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
#include <protura/wait.h>

#include <arch/spinlock.h>
#include <arch/idt.h>
#include <arch/drivers/pic8259.h>
#include <arch/asm.h>
#include <protura/fs/block.h>
#include <protura/drivers/pci.h>
#include <protura/drivers/pci_ids.h>
#include <protura/drivers/ide.h>
#include <protura/drivers/ide_dma.h>

#define KP_IDE 99

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
    IDE_STATUS_DATA_CORRECT = (1 << 2),
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

    IDE_COMMAND_DMA_LBA28_READ = 0xC8,
    IDE_COMMAND_DMA_LBA28_WRITE = 0xCA,

    IDE_COMMAND_CACHE_FLUSH = 0xE7,
};

enum {
    IDE_SECTOR_SIZE = 512,
};

struct ide_state {
    struct spinlock lock;

    int next_is_slave;

    list_head_t block_queue_master;
    list_head_t block_queue_slave;

    size_t block_size[2];

    int use_dma[2];
    struct ide_dma_info dma;
};

static struct ide_state ide_state = {
    .lock = SPINLOCK_INIT("ide_state lock"),
    .next_is_slave = 0,
    .block_queue_master = LIST_HEAD_INIT(ide_state.block_queue_master),
    .block_queue_slave = LIST_HEAD_INIT(ide_state.block_queue_slave),

#ifdef CONFIG_IDE_DMA_SUPPORT
    .use_dma = { 1, 1 },
#else
    .use_dma = { 0, 0 },
#endif
};

#define ide_queue_empty(ide_state) \
    (list_empty(&(ide_state)->block_queue_master) && list_empty(&(ide_state)->block_queue_slave))

static int __ide_read_status(void)
{
    return inb(IDE_PORT_COMMAND_STATUS);
}

static int __ide_wait_ready(void)
{
    int ret;

    do {
        ret = __ide_read_status();
    } while ((ret & (IDE_STATUS_BUSY | IDE_STATUS_READY)) != IDE_STATUS_READY);

    return ret;
}

static void __ide_start_queue(void)
{
    struct block *b;
    sector_t disk_sector;
    int sector_count;
    int use_dma = 0;
    list_head_t *block_queue;


    if (!list_empty(&ide_state.block_queue_master)) {
        block_queue = &ide_state.block_queue_master;
        ide_state.next_is_slave = 0;
    } else {
        block_queue = &ide_state.block_queue_slave;
        ide_state.next_is_slave = 1;
    }

    if (ide_state.use_dma[ide_state.next_is_slave] && ide_state.dma.is_enabled)
        use_dma = 1;

    /* We don't actually remove this block from the queue here. That will be
     * done in the interrupt handler once we're completely done with this block
     */
    b = list_first(block_queue, struct block, block_list_node);

    sector_count = b->block_size / IDE_SECTOR_SIZE;
    disk_sector = b->sector * sector_count;
    /* Start the IDE data transfer */

    /* We select the drive before issuing the __ide_wait_ready()
     *
     * Then we send the sector info - which includes setting the drive-head a
     * second time
     */
    outb(IDE_PORT_DRIVE_HEAD, IDE_DH_SHOULD_BE_SET
                                | IDE_DH_LBA
                                | ((ide_state.next_is_slave)? IDE_DH_SLAVE: 0)
                                );

    __ide_wait_ready();

    //kp(KP_TRACE, "B sector: %d, IDE: sector=%d, master/slave: %d\n", b->sector, disk_sector, ide_state.next_is_slave);

    outb(IDE_PORT_DRIVE_HEAD, IDE_DH_SHOULD_BE_SET
                                | IDE_DH_LBA
                                | ((disk_sector >> 24) & 0x0F)
                                | ((ide_state.next_is_slave)? IDE_DH_SLAVE: 0)
                                );
    outb(IDE_PORT_SECTOR_CNT, sector_count);
    outb(IDE_PORT_LBA_LOW_8, (disk_sector) & 0xFF);
    outb(IDE_PORT_LBA_MID_8, (disk_sector >> 8) & 0xFF);
    outb(IDE_PORT_LBA_HIGH_8, (disk_sector >> 16) & 0xFF);

    if (use_dma) {
        if (flag_test(&b->flags, BLOCK_DIRTY) && ide_dma_setup_write(&ide_state.dma, b) == 0) {
            /* DMA WRITE */
        } else if (!flag_test(&b->flags, BLOCK_VALID) && ide_dma_setup_read(&ide_state.dma, b) == 0) {
            /* DMA READ */
        } else {
            use_dma = 0;
        }
    }

    if (flag_test(&b->flags, BLOCK_DIRTY)) {
        int i;

        if (use_dma) {
            outb(IDE_PORT_COMMAND_STATUS, IDE_COMMAND_DMA_LBA28_WRITE);

            ide_dma_start(&ide_state.dma);
        } else {
            /* Revert to PIO if DMA can't work */
            outb(IDE_PORT_COMMAND_STATUS, IDE_COMMAND_PIO_LBA28_WRITE);

            /* we have to send every sector individually */
            for (i = 0; i < sector_count; i++) {
                if ((__ide_wait_ready() & (IDE_STATUS_ERROR | IDE_STATUS_DRIVE_FAULT)) != 0)
                    break;

                outsl(IDE_PORT_DATA, b->data + i * IDE_SECTOR_SIZE, IDE_SECTOR_SIZE / sizeof(uint32_t));
            }

            /* After writing, we have to flush the write cache */
            outb(IDE_PORT_COMMAND_STATUS, IDE_COMMAND_CACHE_FLUSH);
        }
    } else {
        if (use_dma) {
            outb(IDE_PORT_COMMAND_STATUS, IDE_COMMAND_DMA_LBA28_READ);

            ide_dma_start(&ide_state.dma);
        } else {
            outb(IDE_PORT_COMMAND_STATUS, IDE_COMMAND_PIO_LBA28_READ);
        }
    }
}

static void __ide_handle_intr(struct irq_frame *frame)
{
    struct block *b;
    list_head_t *block_queue;
    int use_dma;

    if (ide_state.next_is_slave)
        block_queue = &ide_state.block_queue_slave;
    else
        block_queue = &ide_state.block_queue_master;

    if (list_empty(block_queue))
        return ;

    use_dma = ide_state.use_dma[ide_state.next_is_slave] && ide_state.dma.is_enabled;

    b = list_take_first(block_queue, struct block, block_list_node);

    if (use_dma) {
        int dma_stat, st;

        dma_stat = ide_dma_check(&ide_state.dma);

        ide_dma_abort(&ide_state.dma);

        st = __ide_read_status();

        if ((dma_stat & 7))
            kp(KP_IDE, "DATA READ ERROR: 0x%02x\n", dma_stat);
        else if ((st & (IDE_STATUS_BUSY | IDE_STATUS_DATA_READ)) != IDE_STATUS_DATA_READ)
            kp(KP_IDE, "DATA STATUS ERROR: 0x%02x\n", st);
    } else {
        /* If we were doing a read, then read the data now. We have to wait until
         * the drive is in the IDE_STATUS_READY state until we can start the read.
         *
         * Note that we don't attempt the read if there is an error reported by the
         * drive (checked via __ide_wait_ready). Also, after reading every sector
         * we have to do another __ide_wait_ready().
         */
        if (!flag_test(&b->flags, BLOCK_DIRTY)) {
            int i;
            int sector_count = b->block_size / IDE_SECTOR_SIZE;

            for (i = 0; i < sector_count; i++) {
                if ((__ide_wait_ready() & (IDE_STATUS_ERROR | IDE_STATUS_DRIVE_FAULT)) != 0)
                    break;

                insl(IDE_PORT_DATA, b->data + i * IDE_SECTOR_SIZE, IDE_SECTOR_SIZE / sizeof(uint32_t));
            }
        }
    }

    /* Our read/write is done, so now our block is definitely valid and not dirty */
    flag_set(&b->flags, BLOCK_VALID);
    flag_clear(&b->flags, BLOCK_DIRTY);

    /* Wakeup the owner of this block */
    scheduler_task_wake(b->owner);

    if (!ide_queue_empty(&ide_state))
        __ide_start_queue();

    return ;
}

static void ide_handle_intr(struct irq_frame *frame, void *param)
{
    using_spinlock(&ide_state.lock)
        __ide_handle_intr(frame);
}

void ide_init(void)
{
    pic8259_enable_irq(IDE_IRQ);
    irq_register_callback(PIC8259_IRQ0 + IDE_IRQ, ide_handle_intr, "IDE", IRQ_INTERRUPT, NULL);

    outb(IDE_PORT_PRIMARY_CTL, 0);
    outb(IDE_PORT_DRIVE_HEAD, IDE_DH_SHOULD_BE_SET | IDE_DH_LBA);

    block_dev_set_block_size(DEV_MAKE(BLOCK_DEV_IDE_MASTER, 0), IDE_SECTOR_SIZE);
    block_dev_set_block_size(DEV_MAKE(BLOCK_DEV_IDE_SLAVE, 0), IDE_SECTOR_SIZE);
}

static void ide_sync_block(struct block *b, int master_or_slave)
{
    /* If the block is valid and not dirty, then there is no syncing needed */
    if (flag_test(&b->flags, BLOCK_VALID) && !flag_test(&b->flags, BLOCK_DIRTY))
        return ;

    using_spinlock(&ide_state.lock) {
        int start = ide_queue_empty(&ide_state);

        list_head_t *list = (master_or_slave)
                            ? &ide_state.block_queue_slave
                            : &ide_state.block_queue_master;

        list_add_tail(list, &b->block_list_node);

        if (start)
            __ide_start_queue();
    }

    sleep_event(flag_test(&b->flags, BLOCK_VALID) && !flag_test(&b->flags, BLOCK_DIRTY));
}

void ide_sync_block_master(struct block_device *__unused dev, struct block *b)
{
    return ide_sync_block(b, 0);
}

void ide_sync_block_slave(struct block_device *__unused dev, struct block *b)
{
    return ide_sync_block(b, 1);
}

struct block_device_ops ide_master_block_device_ops = {
    .sync_block = ide_sync_block_master,
};

struct block_device_ops ide_slave_block_device_ops = {
    .sync_block = ide_sync_block_slave,
};

void ide_dma_device_init(struct pci_dev *dev)
{
    ide_dma_init(&ide_state.dma, dev);
}
