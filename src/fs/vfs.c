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
#include <arch/task.h>

#include <fs/block.h>
#include <fs/super.h>
#include <fs/file.h>
#include <fs/stat.h>
#include <fs/fcntl.h>
#include <fs/inode.h>
#include <fs/inode_table.h>
#include <fs/namei.h>
#include <fs/vfs.h>


int vfs_open(struct inode *inode, unsigned int file_flags, struct file **filp_ret)
{
    int ret = 0;
    struct file *filp;

    *filp_ret = NULL;

    filp = kzalloc(sizeof(*filp), PAL_KERNEL);

    filp->mode = inode->mode;
    filp->inode = inode_dup(inode);
    filp->offset = 0;
    filp->flags = file_flags;
    filp->ops = inode->default_fops;

    atomic_inc(&filp->ref);

    if (file_has_open(filp))
        ret = filp->ops->open(inode, filp);

    if (ret < 0)
        goto cleanup_filp;

    *filp_ret = filp;

    return ret;

  cleanup_filp:
    inode_put(filp->inode);
    kfree(filp);

    return ret;
}

int vfs_close(struct file *filp)
{
    int ret = 0;

    if (atomic_dec_and_test(&filp->ref) != 0)
        return 0;

    if (file_has_release(filp))
        ret = filp->ops->release(filp);

    inode_put(filp->inode);
    kfree(filp);

    return ret;
}

int vfs_read(struct file *filp, void *buf, size_t len)
{
    if (file_has_read(filp))
        return filp->ops->read(filp, buf, len);
    else
        return -ENOTSUP;
}

int vfs_write(struct file *filp, void *buf, size_t len)
{
    if (file_has_write(filp))
        return filp->ops->write(filp, buf, len);
    else
        return -ENOTSUP;
}

off_t vfs_lseek(struct file *filp, off_t off, int whence)
{
    if (file_has_lseek(filp))
        return filp->ops->lseek(filp, off, whence);
    else
        return -ENOTSUP;
}

int vfs_lookup(struct inode *inode, const char *name, size_t len, struct inode **result)
{
    if (inode_has_lookup(inode))
        return inode->ops->lookup(inode, name, len, result);
    else
        return -ENOTSUP;
}

int vfs_truncate(struct inode *inode, off_t length)
{
    if (inode_has_truncate(inode))
        return inode->ops->truncate(inode, length);
    else
        return -ENOTSUP;
}

sector_t vfs_bmap(struct inode *inode, sector_t s)
{
    if (inode_has_bmap(inode))
        return inode->ops->bmap(inode, s);
    else
        return -ENOTSUP;
}

