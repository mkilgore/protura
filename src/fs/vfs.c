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

    kp(KP_TRACE, "Opening file: %p, %d, %p\n", inode, file_flags, filp_ret);

    *filp_ret = NULL;

    filp = kzalloc(sizeof(*filp), PAL_KERNEL);

    kp(KP_TRACE, "Allocated filp: %p\n", filp);

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

    kp(KP_TRACE, "closing file, inode:%d:%d, %d\n", filp->inode->ino, filp->inode->dev, atomic_get(&filp->ref));

    if (!atomic_dec_and_test(&filp->ref))
        return 0;

    kp(KP_TRACE, "Releasing file with inode:%d:%d!\n", filp->inode->ino, filp->inode->dev);

    if (file_has_release(filp))
        ret = filp->ops->release(filp);

    inode_put(filp->inode);

    kp(KP_TRACE, "Freeing file %p\n", filp);
    kfree(filp);

    return ret;
}

int vfs_read(struct file *filp, void *buf, size_t len)
{
    if (S_ISDIR(filp->inode->mode))
        return -EISDIR;

    if (file_has_read(filp))
        return filp->ops->read(filp, buf, len);
    else
        return -ENOTSUP;
}

int vfs_read_dent(struct file *filp, struct dent *dent, size_t size)
{
    if (!S_ISDIR(filp->inode->mode))
        return -ENOTDIR;

    if (file_has_read_dent(filp))
        return filp->ops->read_dent(filp, dent, size);
    else
        return -ENOTSUP;
}

int vfs_write(struct file *filp, void *buf, size_t len)
{
    if (S_ISDIR(filp->inode->mode))
        return -EISDIR;

    if (file_has_write(filp))
        return filp->ops->write(filp, buf, len);
    else
        return -ENOTSUP;
}

off_t vfs_lseek(struct file *filp, off_t off, int whence)
{
    if (S_ISDIR(filp->inode->mode))
        return -EISDIR;

    if (file_has_lseek(filp))
        return filp->ops->lseek(filp, off, whence);
    else
        return -ENOTSUP;
}

int vfs_lookup(struct inode *inode, const char *name, size_t len, struct inode **result)
{
    if (!S_ISDIR(inode->mode))
        return -ENOTDIR;

    if (inode_has_lookup(inode))
        return inode->ops->lookup(inode, name, len, result);
    else
        return -ENOTSUP;
}

int vfs_truncate(struct inode *inode, off_t length)
{
    struct inode_attributes attrs = { 0 };

    attrs.change |= INO_ATTR_SIZE;
    attrs.size = length;

    if (inode_has_change_attrs(inode))
        return inode->ops->change_attrs(inode, &attrs);

    return 0;
}

sector_t vfs_bmap(struct inode *inode, sector_t s)
{
    if (inode_has_bmap(inode))
        return inode->ops->bmap(inode, s);
    else
        return -ENOTSUP;
}

