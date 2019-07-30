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

    filp->inode->atime = protura_current_time_get();
    inode_set_dirty(filp->inode);

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

    old->mtime = old->ctime = protura_current_time_get();
    atomic_inc(&old->nlinks);
    inode_set_dirty(old);

    dir->mtime = dir->ctime = protura_current_time_get();
    inode_set_dirty(dir);

    return 0;
}

static int ext2_dir_unlink(struct inode *dir, struct inode *link, const char *name, size_t len)
{
    int ret;

    kp_ext2(dir->sb, "Unlink: "PRinode" adding %s\n", Pinode(dir), name);

    using_inode_lock_write(dir)
        ret = __ext2_dir_remove(dir, name, len);

    if (ret)
        return ret;

    link->mtime = link->ctime = protura_current_time_get();
    atomic_dec(&link->nlinks);
    inode_set_dirty(link);

    dir->mtime = dir->ctime = protura_current_time_get();
    inode_set_dirty(dir);

    return ret;
}

static int ext2_dir_rmdir(struct inode *dir, struct inode *deldir, const char *name, size_t len)
{
    int ret = 0;
    struct ext2_super_block *sb = container_of(dir->sb, struct ext2_super_block, sb);

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

    using_super_block(dir->sb)
        sb->groups[ext2_ino_group(sb, deldir->ino)].directory_count--;

    /* '..' in deldir */
    inode_dec_nlinks(dir);

    /* '.' in deldir */
    inode_dec_nlinks(deldir);

    /* entry in dir */
    inode_dec_nlinks(deldir);

    dir->ctime = dir->mtime = protura_current_time_get();
    deldir->ctime = deldir->mtime = protura_current_time_get();

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

    using_super_block(dir->sb)
        sb->groups[ext2_ino_group(sb, ino->i.ino)].directory_count++;

    newdir->ctime = newdir->mtime = protura_current_time_get();
    atomic_set(&newdir->nlinks, 2);
    inode_set_valid(newdir);
    inode_set_dirty(newdir);

    dir->ctime = dir->mtime = protura_current_time_get();
    inode_inc_nlinks(dir);

    using_super_block(newdir->sb)
        sb->sb.ops->inode_write(&sb->sb, newdir);

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
    ino->ctime = ino->mtime = protura_current_time_get();
    inode_set_valid(ino);
    inode_set_dirty(ino);
    inode_inc_nlinks(ino);

    dir->ctime = dir->mtime = protura_current_time_get();
    inode_set_dirty(dir);

    kp_ext2(dir->sb, "Inode links: %d\n", atomic32_get(&ino->ref));

    using_super_block(ino->sb)
        ino->sb->ops->inode_write(ino->sb, ino);

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

    inode->ctime = inode->mtime = protura_current_time_get();
    inode_set_valid(inode);
    inode_set_dirty(inode);
    inode_inc_nlinks(inode);

    dir->ctime = dir->mtime = protura_current_time_get();
    inode_set_dirty(dir);

    using_super_block(inode->sb)
        inode->sb->ops->inode_write(inode->sb, inode);

    return 0;
}

/*
 * Check if 'check' is a desendent of base
 */
static int ext2_subdir_check(struct inode *base, struct inode *check)
{
    ino_t base_ino = base->ino;
    struct inode *cur = inode_dup(check);
    int ret = 0;

    if (!S_ISDIR(cur->mode)) {
        ret = -EINVAL;
        goto cleanup_cur;
    }

    while (cur->ino != EXT2_ROOT_INO) {
        ino_t entry;

        if (cur->ino == base_ino) {
            ret = -EINVAL;
            goto cleanup_cur;
        }

        int ret = __ext2_dir_lookup_ino(cur, "..", 2, &entry);
        if (ret) {
            ret = -EINVAL;
            goto cleanup_cur;
        }

        inode_put(cur);

        cur = inode_get(base->sb, entry);
        if (!cur)
            return -EINVAL;
    }

  cleanup_cur:
    inode_put(cur);
    return ret;
}

static int ext2_dir_rename_within_dir(struct inode *dir, struct inode *entry, const char *name, size_t len, const char *new_name, size_t new_len)
{
    int ret;

    using_inode_lock_write(dir) {
        ret = __ext2_dir_remove(dir, name, len);

        if (!ret)
            ret = __ext2_dir_add(dir, new_name, new_len, entry->ino, entry->mode);
    }

    return ret;
}

static inline void swap_inodes(struct inode **i1, struct inode **i2)
{
    struct inode *tmp;
    tmp = *i1;
    *i1 = *i2;
    *i2 = tmp;
}

/*
 * Lookup new_name - if exist, fail
 * Lookup old_name
 *
 * If cross directory:
 *   Lock inodes in inode-pointer order
 *     Remove entry in old_dir
 *     Add entry in new_dir
 *     Modify ".." entry in old_name inode, point to new_dir
 *     dec old_dir nlink
 *     inc new_dir nlink
 *
 * If same directory:
 *   Lock directory inode
 *     Modify entry in old_dir to be new_name
 *
 */
static int ext2_dir_rename(struct inode *old_dir, const char *name, size_t len, struct inode *new_dir, const char *new_name, size_t new_len)
{
    int ret;
    struct inode *entry;
    struct inode *lock1, *lock2, *lock3;

    kp_ext2(old_dir->sb, "Renaming %s in "PRinode" to %s in "PRinode"\n", name, Pinode(old_dir), new_name, Pinode(new_dir));

    using_inode_lock_read(old_dir)
        ret = __ext2_dir_lookup(old_dir, name, len, &entry);

    if (ret)
        return ret;

    if (old_dir == new_dir) {
        ret = ext2_dir_rename_within_dir(old_dir, entry, name, len, new_name, new_len);
        goto cleanup_entry;
    }

    /* Cross directory */

    if (S_ISDIR(entry->mode)) {
        ret = ext2_subdir_check(entry, new_dir);
        if (ret)
            goto cleanup_entry;
    }

    /*
     * We have to lock all of old_dir, entry, and new_dir
     * To avoid deadlocks, these are always locked in inode-pointer order.
     */
    lock1 = old_dir;
    lock2 = entry;
    lock3 = new_dir;

    if (lock1 > lock2)
        swap_inodes(&lock1, &lock2);

    if (lock1 > lock3)
        swap_inodes(&lock1, &lock3);

    if (lock2 > lock3)
        swap_inodes(&lock2, &lock3);

    inode_lock_write(lock1);
    inode_lock_write(lock2);
    inode_lock_write(lock3);

    ret = __ext2_dir_remove(old_dir, name, len);
    if (ret)
        goto cleanup_unlock_inodes;

    ret = __ext2_dir_add(new_dir, new_name, new_len, entry->ino, entry->mode);
    if (ret)
        goto cleanup_unlock_inodes;

    ret = __ext2_dir_change_dotdot(entry, new_dir->ino);

    entry->ctime = entry->mtime = protura_current_time_get();
    new_dir->ctime = new_dir->mtime = protura_current_time_get();
    old_dir->ctime = old_dir->mtime = protura_current_time_get();

  cleanup_unlock_inodes:
    inode_unlock_write(lock3);
    inode_unlock_write(lock2);
    inode_unlock_write(lock1);
  cleanup_entry:
    inode_put(entry);
    return ret;
}

static int ext2_dir_symlink(struct inode *dir, const char *name, size_t len, const char *symlink_target)
{
    int ret;
    size_t target_len;
    struct inode *symlink;
    struct ext2_inode *symlink_ext2;

    target_len = strlen(symlink_target);

    ret = ext2_inode_new(dir->sb, &symlink);
    if (ret)
        return ret;

    kp_ext2(dir->sb, "Creating %s, new symlink: %d, %p\n", name, symlink->ino, symlink);

    symlink->mode = S_IFLNK;
    symlink->ops = &ext2_inode_ops_symlink;
    symlink->default_fops = NULL;
    symlink->size = target_len;

    using_inode_lock_write(dir)
        ret = __ext2_dir_add(dir, name, len, symlink->ino, S_IFLNK);

    if (ret) {
        inode_put(symlink);
        return ret;
    }

    symlink_ext2 = container_of(symlink, struct ext2_inode, i);

    if (target_len <= 59) {
        strcpy((char *)symlink_ext2->blk_ptrs, symlink_target);
        symlink->blocks = 0;
    } else {
        struct block *b;
        sector_t s;

        symlink->blocks = 1;
        s = ext2_bmap_alloc(symlink, 0);

        using_block(symlink->sb->dev, s, b) {
            strcpy(b->data, symlink_target);
            block_mark_dirty(b);
        }
    }

    /* We wait to do this until we're sure the entry succeeded */
    symlink->ctime = symlink->mtime = protura_current_time_get();
    inode_set_valid(symlink);
    inode_set_dirty(symlink);
    inode_inc_nlinks(symlink);

    dir->ctime = dir->mtime = protura_current_time_get();
    inode_set_dirty(dir);

    kp_ext2(dir->sb, "Inode links: %d\n", atomic32_get(&symlink->ref));

    using_super_block(symlink->sb)
        symlink->sb->ops->inode_write(symlink->sb, symlink);

    inode_put(symlink);

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
    .rename = ext2_dir_rename,
    .symlink = ext2_dir_symlink,
};

