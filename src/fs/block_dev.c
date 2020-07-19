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
#include <protura/mm/kmalloc.h>
#include <protura/mm/user_check.h>
#include <protura/fs/block.h>
#include <protura/dev.h>
#include <protura/fs/inode.h>
#include <protura/fs/file.h>
#include <protura/fs/pipe.h>

#include <protura/drivers/ata.h>

int block_dev_pread_generic(struct file *filp, struct user_buffer buf, size_t len, off_t off)
{
    size_t have_read = 0;
    dev_t dev = filp->inode->dev_no;

    if (!file_is_readable(filp))
        return -EBADF;

    /* Guard against reading past the end of the file */
    kp(KP_TRACE, "Block-dev inode size: %ld\n", filp->inode->size);

    size_t total_len = block_dev_get_device_size(dev);
    if (off + len > total_len)
        len = total_len - off;

    kp(KP_TRACE, "len: %d\n", len);
    /* Access the block device for this file, and get it's block size */
    size_t block_size = block_dev_get_block_size(dev);
    sector_t sec = off / block_size;
    off_t sec_off = off - sec * block_size;

    while (have_read < len) {
        struct block *b;

        off_t left = (len - have_read > block_size - sec_off)?
                        block_size - sec_off:
                        len - have_read;

        using_block_locked(dev, sec, b) {
            int ret = user_memcpy_from_kernel(user_buffer_index(buf, have_read), b->data + sec_off, left);
            if (ret)
                return ret;
        }

        have_read += left;

        sec_off = 0;
        sec++;
    }

    filp->offset += have_read;

    return have_read;
}

int block_dev_read_generic(struct file *filp, struct user_buffer buf, size_t len)
{
    int ret = block_dev_pread_generic(filp, buf, len, filp->offset);

    filp->offset += ret;

    return ret;
}

struct file_ops block_dev_file_ops_generic = {
    .open = NULL,
    .release = NULL,
    .pread = block_dev_pread_generic,
    .read = block_dev_read_generic,
    .write = NULL,
    .lseek = fs_file_generic_lseek,
    .readdir = NULL,
};

static mutex_t anon_dev_bitmap_lock = MUTEX_INIT(anon_dev_bitmap_lock);
static int anon_dev_bitmap[256 / sizeof(int) / CHAR_BIT];

dev_t block_dev_anon_get(void)
{
    dev_t ret = 0;

    using_mutex(&anon_dev_bitmap_lock) {
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
    using_mutex(&anon_dev_bitmap_lock) {
        bit_clear(anon_dev_bitmap, bit);
    }
}

static struct block_device devices[] = {
    [BLOCK_DEV_NONE] = {
        .name = "",
        .major = BLOCK_DEV_NONE,
        .partitions = NULL,
        .partition_count = 0,
        .ops = NULL,
        .fops = NULL,
        .blocks = LIST_HEAD_INIT(devices[BLOCK_DEV_NONE].blocks),
    },
    [BLOCK_DEV_IDE_MASTER] = {
        .name = "ide-master",
        .major = BLOCK_DEV_IDE_MASTER,
        .partitions = NULL,
        .partition_count = 0,
        .ops = &ata_master_block_device_ops,
        .fops = &block_dev_file_ops_generic,
        .blocks = LIST_HEAD_INIT(devices[BLOCK_DEV_IDE_MASTER].blocks),
    },
    [BLOCK_DEV_IDE_SLAVE] = {
        .name = "ide-slave",
        .major = BLOCK_DEV_IDE_SLAVE,
        .partitions = NULL,
        .partition_count = 0,
        .ops = &ata_slave_block_device_ops,
        .fops = &block_dev_file_ops_generic,
        .blocks = LIST_HEAD_INIT(devices[BLOCK_DEV_IDE_SLAVE].blocks),
    },
    [BLOCK_DEV_ANON] = {
        .name = "anon",
        .major = BLOCK_DEV_ANON,
        .partitions = NULL,
        .partition_count = 0,
        .ops = NULL,
        .fops = NULL,
        .blocks = LIST_HEAD_INIT(devices[BLOCK_DEV_ANON].blocks),
    },
};


void block_dev_init(void)
{
}

struct block_device *block_dev_get(dev_t device)
{
    int d = DEV_MAJOR(device);

    if (d < ARRAY_SIZE(devices))
        return devices + d;
    else
        return NULL;
}

int block_dev_set_block_size(dev_t device, size_t size)
{
    struct block_device *bdev = block_dev_get(device);

    if (!bdev)
        return -ENODEV;

    int minor = DEV_MINOR(device);

    if (minor == 0) {
        block_dev_clear(device);
        kp(KP_NORMAL, "Block Device %d:%d, block size: %d\n", DEV_MAJOR(device), minor, size);
        bdev->block_size = size;
        return 0;
    }

    if (minor > bdev->partition_count)
        return -ENODEV;

    int partition_no = minor - 1;

    block_dev_clear(device);
    kp(KP_NORMAL, "Block Device %d:%d, partition %d, block size: %d\n", DEV_MAJOR(device), minor, partition_no, size);

    bdev->partitions[partition_no].block_size = size;

    return 0;
}

size_t block_dev_get_block_size(dev_t device)
{
    struct block_device *bdev = block_dev_get(device);

    if (!bdev)
        return 0;

    int minor = DEV_MINOR(device);

    if (minor == 0)
        return bdev->block_size;

    if (minor > bdev->partition_count)
        return 0;

    return bdev->partitions[minor - 1].block_size;
}

size_t block_dev_get_device_size(dev_t device)
{
    struct block_device *bdev = block_dev_get(device);

    if (!bdev)
        return 0;

    int minor = DEV_MINOR(device);

    if (minor == 0)
        return bdev->device_size;

    if (minor > bdev->partition_count)
        return 0;

    return bdev->partitions[minor - 1].device_size;
}
