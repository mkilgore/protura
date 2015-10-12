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

int file_fd_get(int fd, struct file **ret)
{
    struct task *current = cpu_get_local()->current;
    struct file *filp;

    *ret = NULL;
    filp = current->files[fd];

    if (!filp)
        return -EBADF;

    *ret = filp;
    return 0;
}

int file_fd_add(int fd, struct file *filp)
{
    struct task *current = cpu_get_local()->current;

    if (fd >= NOFILE || fd < 0)
        return -EMFILE;

    current->files[fd] = filp;

    return 0;
}

int file_fd_del(int fd)
{
    struct task *current = cpu_get_local()->current;

    if (fd >= NOFILE || fd < 0)
        return -EMFILE;

    current->files[fd] = NULL;

    return 0;
}

/* Generic read implemented using bmap */
int fs_file_generic_read(struct file *filp, void *vbuf, size_t len)
{
    char *buf = vbuf;
    size_t have_read = 0;
    dev_t dev = filp->inode->sb->dev;

    /* Guard against reading past the end of the file */
    if (filp->offset + len > filp->inode->size)
        len = filp->inode->size - filp->offset;

    /* Access the block device for this file, and get it's block size */
    size_t block_size = filp->inode->sb->bdev->block_size;
    sector_t sec = filp->offset / block_size;
    off_t sec_off = filp->offset - sec * block_size;

    while (have_read < len) {
        struct block *b;
        sector_t on_dev = inode_bmap(filp->inode, sec);

        if (on_dev < 0)
            break;

        off_t left = (len - have_read > block_size - sec_off)?
                        block_size - sec_off:
                        len - have_read;

        using_block(dev, on_dev, b)
            memcpy(buf + have_read, b->data + sec_off, left);

        have_read += left;

        sec_off = 0;
        sec++;
    }

    filp->offset += have_read;

    return have_read;
}

int fs_file_generic_write(struct file *filp, void *buf, size_t len)
{
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
    }

    if (filp->offset < 0)
        filp->offset = 0;

    return filp->offset;
}

int sys_read(int fd, void *buf, size_t len)
{
    struct file *filp;
    int ret;

    ret = file_fd_get(fd, &filp);

    if (ret)
        return ret;

    return file_read(filp, buf, len);
}

int sys_write(int fd, void *buf, size_t len)
{
    struct file *filp;
    int ret;

    ret = file_fd_get(fd, &filp);

    if (ret)
        return ret;

    return file_write(filp, buf, len);
}

off_t sys_lseek(int fd, off_t off, int whence)
{
    struct file *filp;
    int ret;

    ret = file_fd_get(fd, &filp);

    if (ret)
        return ret;

    return file_lseek(filp, off, whence);
}

