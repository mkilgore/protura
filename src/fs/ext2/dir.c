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
#include <protura/mutex.h>
#include <protura/dump_mem.h>
#include <protura/mm/kmalloc.h>

#include <arch/spinlock.h>
#include <protura/fs/block.h>
#include <protura/fs/char.h>
#include <protura/fs/stat.h>
#include <protura/fs/file.h>
#include <protura/fs/inode_table.h>
#include <protura/fs/file_system.h>
#include <protura/fs/vfs.h>
#include <protura/fs/ext2.h>
#include "ext2_internal.h"


static int ext2_dir_lookup(struct inode *dir, const char *name, size_t len, struct inode **result)
{
    int ret = 0;

    using_inode_lock_read(dir)
        ret = __ext2_dir_lookup(dir, name, len, result);

    return ret;
}

static int ext2_dir_change_attrs(struct inode *dir, struct inode_attributes *attrs)
{
    if (attrs->change & INO_ATTR_SIZE)
        return -EISDIR;

    return -ENOTSUP;
}

static int ext2_dir_readdir(struct file *filp, struct file_readdir_handler *handler)
{
    int ret = 0;

    using_inode_lock_read(filp->inode)
        ret = __ext2_dir_readdir(filp, handler);

    return ret;
}

static int ext2_dir_read_dent(struct file *filp, struct dent *dent, size_t dent_size)
{
    int ret = 0;

    using_inode_lock_read(filp->inode)
        ret = __ext2_dir_read_dent(filp, dent, dent_size);

    return ret;
}

struct file_ops ext2_file_ops_dir = {
    .open = NULL,
    .release = NULL,
    .read = NULL,
    .write = NULL,
    .lseek = fs_file_generic_lseek,
    .readdir = NULL,
    .read_dent = ext2_dir_read_dent,
};

struct inode_ops ext2_inode_ops_dir = {
    .lookup = ext2_dir_lookup,
    .change_attrs = ext2_dir_change_attrs,
    .bmap = ext2_bmap,
};

