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
#include <protura/fs/block.h>
#include <protura/dev.h>
#include <protura/fs/inode.h>
#include <protura/fs/file.h>
#include <protura/fs/pipe.h>

#include <protura/drivers/ide.h>


int block_dev_read_generic(struct file *filp, void *vbuf, size_t len)
{
    char *buf = vbuf;
    size_t have_read = 0;
    struct block_device *bdev = filp->inode->bdev;
    dev_t dev = filp->inode->dev_no;

    if (!file_is_readable(filp))
        return -EBADF;

    /* Guard against reading past the end of the file */
    if (filp->offset + len > filp->inode->size)
        len = filp->inode->size - filp->offset;

    /* Access the block device for this file, and get it's block size */
    size_t block_size = bdev->block_size;
    sector_t sec = filp->offset / block_size;
    off_t sec_off = filp->offset - sec * block_size;

    while (have_read < len) {
        struct block *b;

        off_t left = (len - have_read > block_size - sec_off)?
                        block_size - sec_off:
                        len - have_read;

        using_block(dev, sec, b)
            memcpy(buf + have_read, b->data + sec_off, left);

        have_read += left;

        sec_off = 0;
        sec++;
    }

    filp->offset += have_read;

    return have_read;
}

struct file_ops block_dev_file_ops_generic = {
    .open = NULL,
    .release = NULL,
    .read = block_dev_read_generic,
    .write = NULL,
    .lseek = fs_file_generic_lseek,
    .readdir = NULL,
};

static struct block_device devices[] = {
    [BLOCK_DEV_NONE] = {
        .name = "",
        .major = BLOCK_DEV_NONE,
        .block_size = 0,
        .ops = NULL,
        .fops = NULL,
        .blocks = LIST_HEAD_INIT(devices[BLOCK_DEV_NONE].blocks),
    },
    [BLOCK_DEV_IDE] = {
        .name = "ide",
        .major = BLOCK_DEV_IDE,
        .block_size = 0,
        .ops = &ide_block_device_ops,
        .fops = &block_dev_file_ops_generic,
        .blocks = LIST_HEAD_INIT(devices[BLOCK_DEV_IDE].blocks),
    },
    [BLOCK_DEV_PIPE] = {
        .name = "pipe",
        .major = BLOCK_DEV_PIPE,
        .block_size = 0,
        .ops = NULL,
        .fops = &pipe_default_file_ops,
        .blocks = LIST_HEAD_INIT(devices[BLOCK_DEV_PIPE].blocks),
    },
};


void block_dev_init(void)
{
    ide_init();
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

    block_dev_clear(device);

    bdev->block_size = size;

    return 0;
}

