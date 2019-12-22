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
#include <protura/drivers/ide.h>
#include "ide.h"

/* The max time we'll wait for the drive to report it is not busy before abandoning it */
#define IDE_MAX_STATUS_TIMEOUT 50

#ifdef CONFIG_KERNEL_LOG_IDE
# define kp_ide(str, ...) kp(KP_NORMAL, "IDE: " str, ## __VA_ARGS__)
#else
# define kp_ide(str, ...) do { ; } while (0)
#endif

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
    IDE_STATUS_DATA_REQUEST = (1 << 3),
    IDE_STATUS_DATA_CORRECT = (1 << 2),
    IDE_STATUS_ERROR = (1 << 0),

    /* This is not a real part of the status, but we report this if the status check hits the timeout */
    IDE_STATUS_TIMEOUT = (1 << 8),
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
    IDE_COMMAND_IDENTIFY = 0xEC,
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
    .lock = SPINLOCK_INIT(),
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


static int __ide_wait_for_status(int status, uint32_t ms)
{
    int ret;
    uint32_t start = timer_get_ms();

    do {
        if (timer_get_ms() - start > ms)
            return IDE_STATUS_TIMEOUT;

        ret = __ide_read_status();
    } while ((ret & (IDE_STATUS_BUSY | status)) != status);

    return ret;
}

static void __ide_do_pio_read(char *buf)
{
    insl(IDE_PORT_DATA, buf, IDE_SECTOR_SIZE / sizeof(uint32_t));
}

static void __ide_do_pio_write(char *buf)
{
    outsl(IDE_PORT_DATA, buf, IDE_SECTOR_SIZE / sizeof(uint32_t));
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

    if (DEV_MINOR(b->dev)) {
        int minor = DEV_MINOR(b->dev);
        int part_no = minor - 1;

        if (part_no < b->bdev->partition_count)
            disk_sector += b->bdev->partitions[part_no].start;
    }

    /* Start the IDE data transfer */

    /* We select the drive before issuing the __ide_wait_for_status(IDE_STATUS_READY, IDE_MAX_STATUS_TIMEOUT)
     *
     * Then we send the sector info - which includes setting the drive-head a
     * second time
     */
    outb(IDE_PORT_DRIVE_HEAD, IDE_DH_SHOULD_BE_SET
                                | IDE_DH_LBA
                                | ((ide_state.next_is_slave)? IDE_DH_SLAVE: 0)
                                );

    int err = __ide_wait_for_status(IDE_STATUS_READY, IDE_MAX_STATUS_TIMEOUT);
    if (err & (IDE_STATUS_ERROR | IDE_STATUS_DRIVE_FAULT | IDE_STATUS_TIMEOUT))
        kp(KP_ERROR, "!!!! IDE DRIVE REPORTED ERROR AFTER SETTING DRIVE HEAD !!!!\n");

    kp_ide("B sector: %d, sector=%d, master/slave: %d, r/w: %s, use_dma: %d\n",
            b->sector,
            disk_sector,
            ide_state.next_is_slave,
            flag_test(&b->flags, BLOCK_DIRTY)? "write": "read",
            use_dma);

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
            ide_dma_start(&ide_state.dma);

            outb(IDE_PORT_COMMAND_STATUS, IDE_COMMAND_DMA_LBA28_WRITE);
        } else {
            /* Revert to PIO if DMA can't work */
            outb(IDE_PORT_COMMAND_STATUS, IDE_COMMAND_PIO_LBA28_WRITE);

            /* we have to send every sector individually */
            for (i = 0; i < sector_count; i++) {
                if ((err = __ide_wait_for_status(IDE_STATUS_DATA_REQUEST, IDE_MAX_STATUS_TIMEOUT)) & (IDE_STATUS_ERROR | IDE_STATUS_DRIVE_FAULT | IDE_STATUS_TIMEOUT)) {
                    kp(KP_ERROR, "IDE: __ide_wait_drq_ms() hit an error while writing, err: 0x%04x!!!\n", err);
                    break;
                }

                __ide_do_pio_write(b->data + i * IDE_SECTOR_SIZE);
            }

            /* After writing, we have to flush the write cache */
            outb(IDE_PORT_COMMAND_STATUS, IDE_COMMAND_CACHE_FLUSH);
        }
    } else {
        if (use_dma) {
            ide_dma_start(&ide_state.dma);

            outb(IDE_PORT_COMMAND_STATUS, IDE_COMMAND_DMA_LBA28_READ);
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

        st = __ide_wait_for_status(0, IDE_MAX_STATUS_TIMEOUT);

        if ((dma_stat & 2)) {
            kp(KP_WARNING, "DATA READ ERROR: 0x%02x\n", dma_stat);
            ide_dma_clear_error(&ide_state.dma);
        }

        if (st & IDE_STATUS_ERROR)
            kp(KP_WARNING, "DATA STATUS ERROR: 0x%02x\n", st);
    } else {
        /* If we were doing a read, then read the data now. We have to wait until
         * the drive is in the IDE_STATUS_DATA_REQUEST state until we can start the read.
         *
         * Note that we don't attempt the read if there is an error reported by the
         * drive (checked via __ide_wait_for_status(). Also, after reading every sector
         * we have to do another __ide_wait_for_status().
         */
        if (!flag_test(&b->flags, BLOCK_DIRTY)) {
            int i;
            int sector_count = b->block_size / IDE_SECTOR_SIZE;
            int err = 0;

            for (i = 0; i < sector_count; i++) {
                if ((err = __ide_wait_for_status(IDE_STATUS_DATA_REQUEST, IDE_MAX_STATUS_TIMEOUT)) & (IDE_STATUS_ERROR | IDE_STATUS_DRIVE_FAULT | IDE_STATUS_TIMEOUT)) {
                    kp(KP_ERROR, "IDE: __ide_wait_drq_ms() hit an error while reading, err: 0x%04x!!!\n", err);
                    break;
                }

                __ide_do_pio_read(b->data + i * IDE_SECTOR_SIZE);
            }
        }
    }

    /* Our read/write is done, so now our block is definitely valid and not dirty */
    flag_set(&b->flags, BLOCK_VALID);
    flag_clear(&b->flags, BLOCK_DIRTY);

    /* Wakeup the owner of this block */
    wait_queue_wake(&b->queue);

    if (!ide_queue_empty(&ide_state))
        __ide_start_queue();

    return ;
}

static void ide_handle_intr(struct irq_frame *frame, void *param)
{
    using_spinlock(&ide_state.lock)
        __ide_handle_intr(frame);
}

static void ide_fix_string(uint8_t *s, size_t len)
{
    if (len % 2)
        panic("Non mod 2 length passed to ide_fix_string()!\n");

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

static void ide_identify(struct block_device *device)
{
    struct page *page = palloc(0, PAL_KERNEL);
    outb(IDE_PORT_COMMAND_STATUS, IDE_COMMAND_IDENTIFY);

    int ret = __ide_wait_for_status(IDE_STATUS_READY, 40);
    if (ret & (IDE_STATUS_DRIVE_FAULT | IDE_STATUS_TIMEOUT) || !(ret & IDE_STATUS_READY))
        goto cleanup;

    __ide_do_pio_read(page->virt);

    struct ide_identify_format *id = page->virt;
    ide_fix_string(id->model, sizeof(id->model));

    kp(KP_NORMAL, "IDENTIFY Model: %s, capacity: %dMB\n", id->model, id->lba_capacity * IDE_SECTOR_SIZE / 1024 / 1024);

    flag_set(&device->flags, BLOCK_DEV_EXISTS);
    device->device_size = id->lba_capacity * IDE_SECTOR_SIZE;

  cleanup:
    pfree(page, 0);
}

static void ide_identify_master(struct block_device *device)
{
    outb(IDE_PORT_DRIVE_HEAD, IDE_DH_SHOULD_BE_SET | IDE_DH_LBA);
    ide_identify(device);
}

static void ide_identify_slave(struct block_device *device)
{
    outb(IDE_PORT_DRIVE_HEAD, IDE_DH_SHOULD_BE_SET | IDE_DH_LBA | IDE_DH_SLAVE);
    ide_identify(device);
}

void ide_init(void)
{
    int i;
    struct block_device *master, *slave;

    int err = irq_register_callback(IDE_IRQ, ide_handle_intr, "IDE", IRQ_INTERRUPT, NULL, 0);
    if (err) {
        kp(KP_WARNING, "IDE: Interrupt %d is already in use!\n", PIC8259_IRQ0 + IDE_IRQ);
        return;
    }

    outb(IDE_PORT_PRIMARY_CTL, 0);
    outb(IDE_PORT_DRIVE_HEAD, IDE_DH_SHOULD_BE_SET | IDE_DH_LBA);

    master = block_dev_get(DEV_MAKE(BLOCK_DEV_IDE_MASTER, 0));
    slave = block_dev_get(DEV_MAKE(BLOCK_DEV_IDE_SLAVE, 0));

    ide_identify_master(master);
    ide_identify_slave(slave);

    if (flag_test(&master->flags, BLOCK_DEV_EXISTS)) {
        block_dev_set_block_size(DEV_MAKE(BLOCK_DEV_IDE_MASTER, 0), IDE_SECTOR_SIZE);
        int master_partition_count = mbr_add_partitions(master);

        kp(KP_NORMAL, "Master partition count: %d\n", master_partition_count);

        for (i = 0; i < master_partition_count; i++)
            master->partitions[i].block_size = IDE_SECTOR_SIZE;
    }

    if (flag_test(&slave->flags, BLOCK_DEV_EXISTS)) {
        block_dev_set_block_size(DEV_MAKE(BLOCK_DEV_IDE_SLAVE, 0), IDE_SECTOR_SIZE);
        int slave_partition_count = mbr_add_partitions(slave);

        kp(KP_NORMAL, "Slave partition count: %d\n", slave_partition_count);

        for (i = 0; i < slave_partition_count; i++)
            slave->partitions[i].block_size = IDE_SECTOR_SIZE;
    }
}

static void ide_sync_block(struct block *b, int master_or_slave)
{
    /* If the block is valid and not dirty, then there is no syncing needed */
    using_spinlock(&ide_state.lock) {
        if (flag_test(&b->flags, BLOCK_VALID) && !flag_test(&b->flags, BLOCK_DIRTY))
            return ;

        int start = ide_queue_empty(&ide_state);

        list_head_t *list = (master_or_slave)
                            ? &ide_state.block_queue_slave
                            : &ide_state.block_queue_master;

        list_add_tail(list, &b->block_list_node);

        if (start)
            __ide_start_queue();
    }

    kp_ide("Waiting on block queue: %p\n", &b->queue);
    wait_queue_event(&b->queue, flag_test(&b->flags, BLOCK_VALID) && !flag_test(&b->flags, BLOCK_DIRTY));
    kp_ide("Block queue done! %p\n", &b->queue);
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
