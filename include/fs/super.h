/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_SUPER_H
#define INCLUDE_FS_SUPER_H

#ifdef __KERNEL__

#include <protura/types.h>
#include <protura/errors.h>
#include <fs/block.h>
#include <fs/dirent.h>

struct inode;
struct super_block;

struct super_block_ops {

    /* Read, write, delete may sleep */
    struct inode *(*inode_read) (struct super_block *, ino_t);
    int (*inode_write) (struct super_block *, struct inode *);
    int (*inode_delete) (struct super_block *, struct inode *);


    /* Release may not sleep - release also does not write */
    int (*inode_release) (struct super_block *, struct inode *);

    int (*sb_write) (struct super_block *);
    int (*sb_put) (struct super_block *);
};

struct super_block {
    dev_t dev;
    struct block_device *bdev;
    struct inode *root;

    struct super_block_ops *ops;
};

static inline struct inode *sb_inode_read(struct super_block *sb, ino_t ino)
{
    if (sb->ops && sb->ops->inode_read)
        return (sb->ops->inode_read) (sb, ino);
    else
        return NULL;
}

static inline int sb_inode_write(struct super_block *sb, struct inode *inode)
{
    if (sb->ops && sb->ops->inode_write)
        return (sb->ops->inode_write) (sb, inode);
    else
        return -ENOTSUP;
}

static inline int sb_inode_release(struct super_block *sb, struct inode *inode)
{
    if (sb->ops && sb->ops->inode_release)
        return (sb->ops->inode_release) (sb, inode);
    else
        return -ENOTSUP;
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

#endif

#endif
