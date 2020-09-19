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
#include <protura/wait.h>

#include <arch/spinlock.h>
#include <protura/block/bdev.h>
#include <protura/block/disk.h>
#include <protura/block/bcache.h>

struct mbr_part {
    uint8_t attributes;

    uint8_t cylinder_s;
    uint8_t head_s;
    uint8_t sector_s;

    uint8_t system_id;

    uint8_t cylinder_e;
    uint8_t head_e;
    uint8_t sector_e;

    /* This is the only real useful part, the CHS addresses are irrevelant */
    uint32_t lba_start;
    uint32_t lba_length;
} __packed;

static int mbr_add_partitions(struct block_device *device)
{
    struct block *b;
    struct page *block_dup;
    size_t i;
    struct disk *disk = device->disk;
    int part_count = 0;

    const int mbr_part_offsets[] = {
        0x1BE,
        0x1CE,
        0x1DE,
        0x1EE,
    };

    block_dup = palloc(0, PAL_KERNEL);

    using_block_locked(device, 0, b)
        memcpy(block_dup->virt, b->data, b->block_size);

    uint8_t *blk = block_dup->virt;

    /* Check for the marker indicating an MBR is present */
    if (blk[510] != 0x55 || blk[511] != 0xAA)
        goto release_copy;

    for (i = 0; i < ARRAY_SIZE(mbr_part_offsets); i++) {
        struct mbr_part *p = (struct mbr_part *)(block_dup->virt + mbr_part_offsets[i]);

        if (p->lba_length) {
            struct disk_part *part = disk_part_alloc();

            part->first_sector = p->lba_start;
            part->sector_count = p->lba_length;

            kp(KP_NORMAL, "Partition for device %d:%d: start: %d, len: %d\n", DEV_MAJOR(device->dev), DEV_MINOR(device->dev), part->first_sector, part->sector_count);
            disk_part_add(disk, part);
            part_count++;
        }
    }

  release_copy:
    pfree(block_dup, 0);
    return part_count;
}

int block_dev_repartition(struct block_device *bdev)
{
    return mbr_add_partitions(bdev);
}
