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
#include <protura/list.h>
#include <mm/kmalloc.h>

#include <arch/spinlock.h>
#include <fs/block.h>
#include <fs/file.h>
#include <fs/file_system.h>
#include <fs/simple_fs.h>
#include <fs/vfs.h>

static int simple_fs_file_write(struct file *filp, void *vbuf, size_t len)
{
    char *buf = vbuf;
    size_t have_written = 0;
    dev_t dev = filp->inode->sb->dev;

    size_t write_len = len;
    if (!file_is_writable(filp))
        return -EBADF;

    if (filp->inode->size - filp->offset > len)
        write_len = filp->inode->size - filp->offset;

    if (len && write_len == 0)
        return -EFBIG;

    if (!len)
        return 0;

    size_t block_size = filp->inode->sb->bdev->block_size;
    sector_t sec = filp->offset / block_size;
    off_t sec_off = filp->offset - sec * block_size;

    using_inode_lock_read(filp->inode) {
        while (have_written < write_len) {
            struct block *b;
            sector_t on_dev = vfs_bmap(filp->inode, sec);

            off_t left = (write_len - have_written > block_size - sec_off)
                         ? block_size - sec_off
                         : write_len - have_written;

            if (on_dev != SECTOR_INVALID) {
                using_block(dev, on_dev, b)
                    memcpy(b->data + sec_off, buf + have_written, left);
            } else {
                kprintf("NEED TO ALLOCATE SECTOR: %d\n", sec);
            }
        }
    }

    filp->offset += have_written;

    return have_written;
}

struct file_ops simple_fs_file_ops = {
    .open = NULL,
    .release = NULL,
    .read = fs_file_generic_read,
    .write = simple_fs_file_write,
    .lseek = fs_file_generic_lseek,
    .readdir = NULL,
};

