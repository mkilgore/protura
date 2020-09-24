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

static int block_dev_pread_generic(struct file *filp, struct user_buffer buf, size_t len, off_t off)
{
    size_t have_read = 0;
    struct block_device *bdev = filp->inode->bdev;

    if (!file_is_readable(filp) || !bdev)
        return -EBADF;

    size_t total_len = block_dev_capacity_get(bdev);
    if (off + len > total_len) {
        /* If the length would be negative, the "seek" is not allowed */
        if (total_len < off)
            return -EOVERFLOW;

        len = total_len - off;
    }

    /* Access the block device for this file, and get it's block size */
    size_t block_size = block_dev_block_size_get(bdev);
    sector_t sec = off / block_size;
    off_t sec_off = off - sec * block_size;

    while (have_read < len) {
        struct block *b;

        off_t left = (len - have_read > block_size - sec_off)?
                        block_size - sec_off:
                        len - have_read;

        using_block_locked(bdev, sec, b) {
            int ret = user_memcpy_from_kernel(user_buffer_index(buf, have_read), b->data + sec_off, left);
            if (ret)
                return ret;
        }

        have_read += left;

        sec_off = 0;
        sec++;
    }

    kp(KP_NORMAL, "block: pread: have_read: %d\n", have_read);
    filp->offset += have_read;

    return have_read;
}

static int block_dev_read_generic(struct file *filp, struct user_buffer buf, size_t len)
{
    int ret = block_dev_pread_generic(filp, buf, len, filp->offset);

    if (ret > 0)
        filp->offset += ret;

    return ret;
}

static int block_dev_write_generic(struct file *filp, struct user_buffer buf, size_t len)
{
    size_t have_written = 0;
    struct block_device *bdev = filp->inode->bdev;
    off_t off = filp->offset;

    if (!file_is_writable(filp) || !bdev)
        return -EBADF;

    size_t total_len = block_dev_capacity_get(bdev);
    if (off + len > total_len) {
        /* If the length would be negative, the "seek" is not allowed */
        if (total_len < off)
            return -EOVERFLOW;

        len = total_len - off;
    }

    /* Access the block device for this file, and get it's block size */
    size_t block_size = block_dev_block_size_get(bdev);
    sector_t sec = off / block_size;
    off_t sec_off = off - sec * block_size;

    while (have_written < len) {
        struct block *b;

        off_t left = (len - have_written > block_size - sec_off)?
                        block_size - sec_off:
                        len - have_written;

        using_block_locked(bdev, sec, b) {
            int ret = user_memcpy_to_kernel(b->data + sec_off, user_buffer_index(buf, have_written), left);
            block_mark_dirty(b);
            if (ret)
                return ret;
        }

        have_written += left;

        sec_off = 0;
        sec++;
    }

    kp(KP_NORMAL, "block: write: have_written: %d\n", have_written);
    filp->offset += have_written;

    return have_written;
}

static int block_dev_fops_open(struct inode *inode, struct file *filp)
{
    return block_dev_open(inode->bdev, filp->flags);
}

static int block_dev_fops_close(struct file *filp)
{
    block_dev_close(filp->inode->bdev);
    return 0;
}

struct file_ops block_dev_file_ops = {
    .open = block_dev_fops_open,
    .release = block_dev_fops_close,
    .pread = block_dev_pread_generic,
    .read = block_dev_read_generic,
    .write = block_dev_write_generic,
    .lseek = fs_file_generic_lseek,
};
