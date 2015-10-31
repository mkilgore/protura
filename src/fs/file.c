/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/list.h>
#include <protura/hlist.h>
#include <protura/string.h>
#include <arch/spinlock.h>
#include <protura/atomic.h>
#include <mm/kmalloc.h>

#include <fs/block.h>
#include <fs/super.h>
#include <fs/inode.h>
#include <fs/file.h>
#include <fs/vfs.h>

/* Generic read implemented using bmap */
int fs_file_generic_read(struct file *filp, void *vbuf, size_t len)
{
    char *buf = vbuf;
    size_t have_read = 0;
    dev_t dev = filp->inode->sb->dev;

    if (!file_is_readable(filp))
        return -EBADF;

    kprintf("Reading!\n");

    /* Guard against reading past the end of the file */
    if (filp->offset + len > filp->inode->size)
        len = filp->inode->size - filp->offset;

    /* Access the block device for this file, and get it's block size */
    size_t block_size = filp->inode->sb->bdev->block_size;
    sector_t sec = filp->offset / block_size;
    off_t sec_off = filp->offset - sec * block_size;

    kprintf("Locking for read inode\n");
    using_inode_lock_read(filp->inode) {
        while (have_read < len) {
            struct block *b;
            sector_t on_dev = vfs_bmap(filp->inode, sec);

            off_t left = (len - have_read > block_size - sec_off)?
                            block_size - sec_off:
                            len - have_read;

            kprintf("Sec: %d, On dev: %d\n", sec, on_dev);

            /* Invalid sectors are treated as though they're a block of all zeros.
             *
             * This allows 'sparse' files, which don't have sectors allocated for
             * every position to save space. */
            if (on_dev != SECTOR_INVALID)
                using_block(dev, on_dev, b)
                    memcpy(buf + have_read, b->data + sec_off, left);
            else
                memset(buf + have_read, 0, left);

            have_read += left;

            sec_off = 0;
            sec++;
        }
    }

    filp->offset += have_read;

    return have_read;
}

int fs_file_generic_write(struct file *filp, void *buf, size_t len)
{
    if (!file_is_writable(filp))
        return -EBADF;

    /* To be implemented */
    return 0;
}

off_t fs_file_generic_lseek(struct file *filp, off_t off, int whence)
{
    switch (whence) {
    case SEEK_SET:
        filp->offset = off;
        break;

    case SEEK_END:
        filp->offset = filp->inode->size;
        break;

    case SEEK_CUR:
        filp->offset += off;
        break;

    default:
        return -EINVAL;
    }

    if (filp->offset < 0)
        filp->offset = 0;

    return filp->offset;
}

struct file *file_dup(struct file *filp)
{
    atomic_inc(&filp->ref);
    return filp;
}

