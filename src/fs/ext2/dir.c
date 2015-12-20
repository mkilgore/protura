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

static int ext2_dir_truncate(struct inode *dir, off_t size)
{
    return -EISDIR;
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

static int ext2_dir_link(struct inode *dir, struct inode *old, const char *name, size_t len)
{
    int ret;

    kp_ext2(dir->sb, "link: %d -> %d(%s)\n", old->ino, dir->ino, name);

    if (S_ISDIR(old->mode))
        return -EPERM;

    using_inode_lock_write(dir) {
        int exists = __ext2_dir_entry_exists(dir, name, len);

        kp_ext2(dir->sb, "Exists: %d\n", exists);

        if (!exists)
            ret = __ext2_dir_add(dir, name, len, old->ino, old->mode);
        else
            ret = -EEXIST;
    }

    if (ret)
        return ret;

    atomic_inc(&old->nlinks);
    inode_set_dirty(old);

    return 0;
}

static int ext2_dir_mkdir(struct inode *dir, const char *name, size_t len, mode_t mode)
{

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
    .truncate = ext2_dir_truncate,
    .bmap = ext2_bmap,
    .bmap_alloc = NULL,
    .link = ext2_dir_link,
};

