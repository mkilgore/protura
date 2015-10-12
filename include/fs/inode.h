/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_INODE_H
#define INCLUDE_FS_INODE_H

#include <protura/types.h>
#include <protura/errors.h>
#include <protura/list.h>
#include <protura/hlist.h>
#include <protura/atomic.h>
#include <protura/semaphore.h>

struct inode_ops;
struct super_block;
struct file;

struct inode {
    ino_t ino;
    dev_t dev;
    off_t size;
    mode_t mode;

    uint32_t valid :1;
    uint32_t dirty :1;

    mutex_t lock;

    atomic_t ref;

    struct super_block *sb;
    struct file_ops *default_file_ops;
    struct inode_ops *ops;

    struct hlist_node hash_entry;
};

struct inode_ops {
    int (*file_open) (struct inode *, struct file **);
    int (*file_close) (struct inode *, struct file *);

    int (*truncate) (struct inode *, off_t len);
    int (*lookup) (struct inode *dir, const char *name, size_t len, struct inode **result);
    sector_t (*bmap) (struct inode *, sector_t);
};

struct inode *inode_get(struct super_block *, ino_t ino);
struct inode *inode_dup(struct inode *);
void inode_put(struct inode *);
int inode_lookup_generic(struct inode *dir, const char *name, size_t len, struct inode **result);

int fd_open(struct inode *inode, struct file **filp);
int fd_close(int fd);

static inline void inode_lock(struct inode *inode)
{
    mutex_lock(&inode->lock);
}

static inline void inode_unlock(struct inode *inode)
{
    mutex_unlock(&inode->lock);
}

static inline int inode_file_open(struct inode *inode, struct file **filp)
{
    if (inode->ops && inode->ops->file_open)
        return (inode->ops->file_open) (inode, filp);
    else
        return -ENOTSUP;
}

static inline int inode_file_close(struct inode *inode, struct file *filp)
{
    if (inode->ops && inode->ops->file_close)
        return (inode->ops->file_close) (inode, filp);
    else
        return -ENOTSUP;
}

static inline int inode_lookup(struct inode *inode, const char *name, size_t len, struct inode **result)
{
    if (inode->ops && inode->ops->lookup)
        return (inode->ops->lookup) (inode, name, len, result);
    else
        return -ENOTSUP;
}

static inline int inode_truncate(struct inode *inode, off_t len)
{
    if (inode->ops && inode->ops->truncate)
        return (inode->ops->truncate) (inode, len);
    else
        return -ENOTSUP;
}

static inline sector_t inode_bmap(struct inode *inode, sector_t sec)
{
    if (inode->ops && inode->ops->bmap)
        return (inode->ops->bmap) (inode, sec);
    else
        return -ENOTSUP;
}

#define using_inode_lock(inode) \
    using_nocheck(inode_lock(inode), inode_unlock(inode))

#define using_inode(sb, ino, inode) \
    using((inode = inode_get(sb, ino)) != NULL, inode_put(inode))

#endif
