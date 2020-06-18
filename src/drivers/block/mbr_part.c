/*
 * Copyright (C) 2019 Matt Kilgore
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
#include <protura/fs/block.h>
#include "ide.h"

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

void make_part(struct mbr_part *mbr, struct partition *part)
{
    partition_init(part);
    part->start = mbr->lba_start;
    part->device_size = mbr->lba_length * IDE_SECTOR_SIZE;

    kp(KP_NORMAL, "  PART Start: %d, Length: %d\n", part->start, part->device_size);
}

int mbr_add_partitions(struct block_device *device)
{
    struct block *b;
    struct page *block_dup;
    size_t i;
    int partition_count = 0;
    int count = 0;
    dev_t raw = DEV_MAKE(device->major, 0);

    const int mbr_part_offsets[] = {
        0x1BE,
        0x1CE,
        0x1DE,
        0x1EE,
    };

    block_dup = palloc(0, PAL_KERNEL);

    using_block_locked(raw, 0, b)
        memcpy(block_dup->virt, b->data, b->block_size);

    for (i = 0; i < ARRAY_SIZE(mbr_part_offsets); i++) {
        struct mbr_part *p = (struct mbr_part *)(block_dup->virt + mbr_part_offsets[i]);
        if (p->lba_length)
            partition_count++;
    }

    if (!partition_count)
        goto cleanup;

    device->partitions = kmalloc(partition_count * sizeof(*device->partitions), PAL_KERNEL);
    device->partition_count = partition_count;

    kp(KP_NORMAL, "Partitions for device: %d: \n", device->major);
    for (i = 0; i < ARRAY_SIZE(mbr_part_offsets); i++) {
        struct mbr_part *p = (struct mbr_part *)(block_dup->virt + mbr_part_offsets[i]);

        if (p->lba_length) {
            make_part(p, device->partitions + count);
            count++;
        }
    }

  cleanup:
    pfree(block_dup, 0);
    return partition_count;
}
