/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_SUPER_H
#define INCLUDE_FS_SUPER_H

#include <protura/types.h>
#include <protura/bits.h>
#include <protura/mutex.h>
#include <protura/errors.h>
#include <protura/block/bdev.h>
#include <protura/fs/dirent.h>

struct inode;
struct super_block;
struct vfs_mount;

struct super_block_ops {
    /* Used to allocate and deallocate 'struct inode's for a given super_block.
     * This is necessary because every super_block uses it's own extension on
     * the basic 'struct inode. Note that these do no reading or writing to the
     * disk, and are purely memory management functions. */
    int (*inode_dealloc) (struct super_block *, struct inode *);
    struct inode *(*inode_alloc) (struct super_block *);

    /* Read, write, and delete may sleep */
    int (*inode_read) (struct super_block *, struct inode *);
    int (*inode_write) (struct super_block *, struct inode *);

    /* called when nlink and the in-kernel reference count of the inode drops
     * to zero.
     *
     * Note that the passed inode is already locked for writing. */
    int (*inode_delete) (struct super_block *, struct inode *);

    /* umount_lock is locked while sb_write is called */
    int (*sb_write) (struct super_block *);

    /* umount_lock is locked while sb_put is called */
    int (*sb_put) (struct super_block *);
};

enum {
    SUPER_IS_VALID,
    SUPER_IS_DEAD,
};

struct super_block {
    int count;

    mutex_t umount_lock;

    flags_t flags;

    struct block_device *bdev;
    ino_t root_ino;

    struct file_system *fs;

    /* Entry into list of all the current super_blocks */
    list_node_t list_entry;

    /* Protected by the inode_hashes_lock */
    list_head_t inodes;
    list_head_t dirty_inodes;

    struct super_block_ops *ops;
};

#define SUPER_BLOCK_INIT(super_block) \
    { \
        .count = 0, \
        .umount_lock = MUTEX_INIT((super_block).umount_lock), \
        .bdev = NULL, \
        .list_entry = LIST_NODE_INIT((super_block).list_entry), \
        .inodes = LIST_HEAD_INIT((super_block).inodes), \
        .dirty_inodes = LIST_HEAD_INIT((super_block).dirty_inodes), \
        .ops = NULL, \
    }

static inline void super_block_init(struct super_block *sb)
{
    *sb = (struct super_block)SUPER_BLOCK_INIT(*sb);
}

static inline int sb_inode_read(struct super_block *sb, struct inode *inode)
{
    if (sb->ops && sb->ops->inode_read)
        return (sb->ops->inode_read) (sb, inode);
    else
        return -ENOTSUP;
}

static inline int sb_inode_write(struct super_block *sb, struct inode *inode)
{
    if (sb->ops && sb->ops->inode_write)
        return (sb->ops->inode_write) (sb, inode);
    else
        return -ENOTSUP;
}

static inline int sb_inode_dealloc(struct super_block *sb, struct inode *inode)
{
    if (sb->ops && sb->ops->inode_dealloc)
        return (sb->ops->inode_dealloc) (sb, inode);
    else
        return -ENOTSUP;
}

static inline struct inode *sb_inode_alloc(struct super_block *sb)
{
    if (sb->ops && sb->ops->inode_alloc)
        return (sb->ops->inode_alloc) (sb);
    else
        return NULL;
}

static inline int sb_inode_delete(struct super_block *sb, struct inode *inode)
{
    if (sb->ops && sb->ops->inode_delete)
        return (sb->ops->inode_delete) (sb, inode);
    else
        return -ENOTSUP;
}

static inline int sb_write(struct super_block *sb)
{
    if (sb->ops && sb->ops->sb_write)
        return (sb->ops->sb_write) (sb);
    else
        return -ENOTSUP;
}

static inline int sb_put(struct super_block *sb)
{
    if (sb->ops && sb->ops->sb_put)
        return (sb->ops->sb_put) (sb);
    else
        return -ENOTSUP;
}

void sync_all_supers(void);
int mount_root(dev_t device, const char *fsystem);

extern const struct file_ops mount_file_ops;

#endif
