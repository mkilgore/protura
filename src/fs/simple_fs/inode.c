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
#include <fs/file_system.h>
#include <fs/simple_fs.h>
#include <fs/file.h>

static int simple_fs_file_open(struct inode *i, struct file **result)
{
    struct file *filp;

    filp = kmalloc(sizeof(*filp), PAL_KERNEL);

    filp->mode = i->mode;
    filp->inode = inode_dup(i);
    filp->offset = 0;
    filp->ops = i->default_file_ops;

    atomic_inc(&filp->ref);

    *result = filp;

    return 0;
}

static int simple_fs_file_close(struct inode *i, struct file *filp)
{

    if (atomic_dec_and_test(&filp->ref) != 0)
        return 0;

    inode_put(filp->inode);
    kfree(filp);

    return 0;
}

static int simple_fs_truncate(struct inode *i, off_t len)
{
    i->size = len;

    return 0;
}

static sector_t simple_fs_bmap(struct inode *i, sector_t sec)
{
    struct simple_fs_inode *inode = container_of(i, struct simple_fs_inode, i);

    if (sec > 12)
        return -1;

    return inode->contents[sec];
}

struct inode_ops simple_fs_inode_ops = {
    .file_open = simple_fs_file_open,
    .file_close = simple_fs_file_close,
    .truncate = simple_fs_truncate,
    .bmap = simple_fs_bmap,
};

