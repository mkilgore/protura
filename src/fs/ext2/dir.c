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

/* Not yet functioning */
#if 0
static int ext2_dir_readdir(struct file *filp, struct file_readdir_handler *handler)
{
    int ret = 0;

    using_inode_lock_read(filp->inode)
        ret = __ext2_dir_readdir(filp, handler);

    return ret;
}
#endif

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

    kp_ext2(dir->sb, "Link: "PRinode" adding %s\n", Pinode(dir), name);

    if (S_ISDIR(old->mode))
        return -EPERM;

    if (atomic_get(&old->nlinks) >= EXT2_LINK_MAX)
        return -EMLINK;

    using_inode_lock_write(dir) {
        int exists = __ext2_dir_entry_exists(dir, name, len);

        if (!exists)
            ret = __ext2_dir_add(dir, name, len, old->ino, old->mode);
        else
            ret = -EEXIST;
    }

    if (ret)
        return ret;

    inode_inc_nlinks(old);

    return 0;
}

static int ext2_dir_unlink(struct inode *dir, struct inode *link, const char *name, size_t len)
{
    int ret;

    kp_ext2(dir->sb, "Unlink: "PRinode" adding %s\n", Pinode(dir), name);

    using_inode_lock_write(dir)
        ret = __ext2_dir_remove(dir, name, len);

    if (ret)
        goto cleanup_link;

    inode_dec_nlinks(link);

  cleanup_link:
    inode_put(link);
    return ret;
}

static int ext2_dir_rmdir(struct inode *dir, struct inode *deldir, const char *name, size_t len)
{
    int ret = 0;

    kp_ext2(dir->sb, "Unlink: "PRinode" adding %s\n", Pinode(dir), name);

    if (!S_ISDIR(deldir->mode))
        return -ENOTDIR;

    using_inode_lock_read(deldir)
        ret = __ext2_dir_empty(deldir);

    if (ret)
        return ret;

    using_inode_lock_write(dir)
        ret = __ext2_dir_remove(dir, name, len);

    if (ret)
        return ret;

    /* '..' in deldir */
    inode_dec_nlinks(dir);

    /* '.' in deldir */
    inode_dec_nlinks(deldir);

    /* entry in dir */
    inode_dec_nlinks(deldir);

    return ret;
}

static int ext2_dir_mkdir(struct inode *dir, const char *name, size_t len, mode_t mode)
{
    int ret;
    int exists;
    struct inode *newdir;
    struct ext2_inode *ino;
    struct ext2_super_block *sb = container_of(dir->sb, struct ext2_super_block, sb);

    mode &= ~S_IFMT;
    mode |= S_IFDIR;

    using_inode_lock_write(dir)
        exists = __ext2_dir_entry_exists(dir, name, len);

    if (exists)
        return -EEXIST;

    if (atomic_get(&dir->nlinks) >= EXT2_LINK_MAX)
        return -EMLINK;

    ret = ext2_inode_new(dir->sb, &newdir);

    if (!newdir)
        return ret;

    newdir->mode = mode;
    newdir->ops = &ext2_inode_ops_dir;
    newdir->default_fops = &ext2_file_ops_dir;
    newdir->size = 0;

    ino = container_of(newdir, struct ext2_inode, i);

    using_inode_lock_write(newdir) {
        ret = __ext2_dir_add(newdir, ".", 1, newdir->ino, newdir->mode);
        ret = __ext2_dir_add(newdir, "..", 2, dir->ino, dir->mode);
    }

    if (ret)
        goto cleanup_newdir;

    using_inode_lock_write(dir)
        ret = __ext2_dir_add(dir, name, len, newdir->ino, newdir->mode);

    if (ret)
        goto cleanup_newdir;

    using_ext2_block_groups(sb)
        sb->groups[ext2_ino_group(sb, ino->i.ino)].directory_count++;

    atomic_set(&newdir->nlinks, 2);
    inode_set_valid(newdir);
    inode_set_dirty(newdir);

    inode_inc_nlinks(dir);

  cleanup_newdir:
    inode_put(newdir);
    return ret;
}

static int ext2_dir_create(struct inode *dir, const char *name, size_t len, mode_t mode, struct inode **result)
{
    int ret;
    struct inode *ino;

    mode &= ~S_IFMT;
    mode |= S_IFREG;

    ret = ext2_inode_new(dir->sb, &ino);
    if (ret)
        return ret;

    kp_ext2(dir->sb, "Creating %s, new inode: %d, %p\n", name, ino->ino, ino);

    ino->mode = mode;
    ino->ops = &ext2_inode_ops_file;
    ino->default_fops = &ext2_file_ops_file;
    ino->size = 0;

    using_inode_lock_write(dir)
        ret = __ext2_dir_add(dir, name, len, ino->ino, mode);

    if (ret) {
        inode_put(ino);
        return ret;
    }

    /* We wait to do this until we're sure the entry succeeded */
    inode_set_valid(ino);
    inode_set_dirty(ino);
    inode_inc_nlinks(ino);

    kp_ext2(dir->sb, "Inode links: %d\n", atomic32_get(&ino->ref));

    if (result)
        *result = ino;

    return 0;
}

static int ext2_dir_mknod(struct inode *dir, const char *name, size_t len, mode_t mode, dev_t dev)
{
    int ret;
    struct inode *inode = NULL;

    ret = ext2_inode_new(dir->sb, &inode);
    if (ret)
        return ret;

    kp_ext2(dir->sb, "Creating %s, new node:", name);

    inode->mode = mode;
    inode->dev_no = dev;

    ext2_inode_setup_ops(inode);

    using_inode_lock_write(dir)
        ret = __ext2_dir_add(dir, name, len, inode->ino, mode);

    if (ret) {
        inode_put(inode);
        return ret;
    }

    inode_set_valid(inode);
    inode_set_dirty(inode);
    inode_inc_nlinks(inode);

    return 0;
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
    .unlink = ext2_dir_unlink,
    .create = ext2_dir_create,
    .mkdir = ext2_dir_mkdir,
    .mknod = ext2_dir_mknod,
    .rmdir = ext2_dir_rmdir,
};

