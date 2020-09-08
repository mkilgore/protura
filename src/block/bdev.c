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
#include <protura/snprintf.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/user_check.h>
#include <protura/dev.h>
#include <protura/fs/inode.h>
#include <protura/fs/file.h>
#include <protura/fs/pipe.h>
#include <protura/block/bcache.h>
#include <protura/block/disk.h>
#include <protura/block/bdev.h>

#define BDEV_HASH_TABLE_SIZE 256
static spinlock_t bdev_cache_lock = SPINLOCK_INIT();
static struct hlist_head bdev_cache[BDEV_HASH_TABLE_SIZE];

static int bdev_hash(dev_t dev)
{
    return (DEV_MAJOR(dev) | DEV_MINOR(dev)) % BDEV_HASH_TABLE_SIZE;
}

static struct block_device *bdev_alloc(void)
{
    struct block_device *bdev = kzalloc(sizeof(*bdev), PAL_KERNEL);
    list_head_init(&bdev->blocks);
    mutex_init(&bdev->lock);
    spinlock_init(&bdev->block_size_lock);

    return bdev;
}

static void bdev_free(struct block_device *bdev)
{
    kfree(bdev);
}

struct block_device *__find_block_dev(dev_t dev)
{
    int hash = bdev_hash(dev);
    struct block_device *bdev;

    hlist_foreach_entry(bdev_cache + hash, bdev, hash_entry)
        if (bdev->dev == dev)
            return bdev;

    return NULL;
}

struct block_device *block_dev_get(dev_t device)
{
    int hash = bdev_hash(device);
    struct block_device *bdev, *new = NULL;

    using_spinlock(&bdev_cache_lock) {
        bdev = __find_block_dev(device);

        if (bdev) {
            bdev->refs++;
            return bdev;
        }
    }

    new = bdev_alloc();
    new->dev = device;

    using_spinlock(&bdev_cache_lock) {
        bdev = __find_block_dev(device);

        if (!bdev) {
            hlist_add(bdev_cache + hash, &new->hash_entry);
            new->refs++;
            return new;
        }

        bdev_free(new);

        bdev->refs++;
        return bdev;
    }
}

void block_dev_put(struct block_device *bdev)
{
    using_spinlock(&bdev_cache_lock)
        bdev->refs--;
}

static void block_dev_create_name(struct block_device *bdev, int partno)
{
    if (!partno) {
        strcpy(bdev->name, bdev->disk->name);
        return;
    }

    snprintf(bdev->name, sizeof(bdev->name), "%s%d", bdev->disk->name, partno);
}

/* Performs various disk actions when attempting to do the first open for the device */
static int block_dev_first_open(struct block_device *bdev, flags_t file_flags)
{
    int ret = 0;
    int partno = -1;
    struct disk *disk = disk_get(bdev->dev, &partno);

    if (!disk)
        return -ENXIO;

    bdev->disk = disk;

    /* This block-device is for a partition and not the whole disk - grab an
     * open_ref to the whole block-device first before continuing */
    if (partno) {
        struct block_device *whole = block_dev_get(disk_dev_get(disk, 0));

        /* The whole should exist, but if a weird race happens we can't
         * continue if it doesn't */
        if (!whole) {
            ret = -ENXIO;
            goto put_disk;
        }

        ret = block_dev_open(whole, 0);
        if (ret) {
            block_dev_put(whole);
            goto put_disk;
        }

        bdev->whole = whole;

        if (disk_open(disk)) {
            /* could happen if the disk is removed */
            ret = -ENXIO;
            block_dev_put(whole);
            goto put_disk;
        }

        bdev->part = disk_part_get(disk, partno);
        if (!bdev->part) {
            ret = -ENXIO;
            disk_close(disk);
            block_dev_put(whole);
            goto put_disk;
        }

        block_dev_create_name(bdev, partno);
    } else {
        /* Initializing the whole disk for the first time */
        if (!flag_test(&bdev->flags, BDEV_INITIALIZED)) {
            ret = disk_part_clear(disk);
            if (!ret) {
                /* Set the block size early, because partitioning require reading the disk */
                bdev->block_size = 1 << bdev->disk->min_block_size_shift;

                block_dev_repartition(bdev);

                flag_set(&bdev->flags, BDEV_INITIALIZED);
            } else {
                kp(KP_WARNING, "There was an open reference to %s when initializing the block device, unable to repartition!\n", disk->name);
            }
        }

        ret = disk_open(disk);
        if (ret)
            goto put_disk;

        bdev->part = disk_part_get(disk, 0);
        if (!bdev->part) {
            ret = -ENXIO;
            disk_close(disk);
            goto put_disk;
        }

        block_dev_create_name(bdev, 0);
    }

    return 0;

  put_disk:
    disk_put(disk);
    bdev->disk = NULL;
    return ret;
}

/* NOTE! This is recursive for a single time, a partition will open the
 * block_device referring to the whole disk. The locks then also nest,  */
int block_dev_open(struct block_device *bdev, flags_t file_flags)
{
    using_mutex(&bdev->lock) {
        kassert(bdev->open_refs >= 0, "Block device %s has open_refs < 0!!!", bdev->name);

        if (!bdev->open_refs) {
            int err = block_dev_first_open(bdev, file_flags);
            if (err)
                return err;

            /* We can do this since we're guaranteed to have no blocks */
            bdev->block_size = 1 << bdev->disk->min_block_size_shift;
        }

        bdev->open_refs++;
    }

    return 0;
}

void block_dev_close(struct block_device *bdev)
{
    using_mutex(&bdev->lock) {
        bdev->open_refs--;
        kassert(bdev->open_refs >= 0, "Block device %s has open_refs < 0!!!", bdev->name);

        if (!bdev->open_refs) {
            block_dev_clear(bdev);

            if (bdev->whole) {
                block_dev_close(bdev->whole);
                block_dev_put(bdev->whole);
            }

            if (bdev->disk) {
                disk_close(bdev->disk);
                disk_put(bdev->disk);
            }

            bdev->disk = NULL;
            bdev->whole = NULL;
            bdev->part = NULL;
        }
    }
}

void block_dev_block_size_set(struct block_device *bdev, size_t size)
{
    block_dev_clear(bdev);

    using_spinlock(&bdev->block_size_lock)
        bdev->block_size = size;
}

size_t block_dev_block_size_get(struct block_device *bdev)
{
    using_spinlock(&bdev->block_size_lock)
        return bdev->block_size;
}

uint64_t block_dev_capacity_get(struct block_device *bdev)
{
    using_mutex(&bdev->lock) {
        if (bdev->disk) {
            return disk_capacity_get(bdev->disk);
        }

        return 0;
    }
}

void block_submit(struct block *b)
{
    /* 
     * Blocks can only exist on block_device's that have actually been opened.
     * As a consequence, we can read b->bdev->disk without taking the lock.
     */
    struct disk *disk = b->bdev->disk;
    struct disk_part *part = b->bdev->part;

    /* We convert the block number that's in 'block-sized' increments into a
     * real sector offset for the disk that is in disk->min_block_size_shift
     * increments */
    b->real_sector = b->sector * (b->block_size >> disk->min_block_size_shift);

    if (part)
        b->real_sector += part->first_sector;

    disk->ops->sync_block(disk, b);
}

static spinlock_t anon_dev_bitmap_lock = SPINLOCK_INIT();
static int anon_dev_bitmap[256 / sizeof(int) / CHAR_BIT];

dev_t block_dev_anon_get(void)
{
    dev_t ret = 0;

    using_spinlock(&anon_dev_bitmap_lock) {
        int bit = bit_find_first_zero(anon_dev_bitmap, sizeof(anon_dev_bitmap));
        if (bit != -1) {
            bit_set(anon_dev_bitmap, bit);
            ret = DEV_MAKE(BLOCK_DEV_ANON, bit);
        }
    }

    return ret;
}

void block_dev_anon_put(dev_t dev)
{
    int bit = DEV_MINOR(dev);
    using_spinlock(&anon_dev_bitmap_lock)
        bit_clear(anon_dev_bitmap, bit);
}
