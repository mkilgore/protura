/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_INODE_H
#define INCLUDE_FS_INODE_H

struct inode;

#include <protura/types.h>
#include <protura/list.h>
#include <protura/hlist.h>
#include <protura/atomic.h>
#include <protura/semaphore.h>

struct inode_ops;
struct super_block;


struct inode {
    kino_t ino;
    kdev_t dev;
    koff_t size;
    kumode_t mode;

    uint32_t valid :1;
    uint32_t dirty :1;

    mutex_t lock;

    atomic32_t ref;

    struct super_block *sb;
    struct inode_ops *ops;

    struct hlist_node hash_entry;
};

struct inode_ops {
    struct file_ops *default_file_ops;
    int (*create) (struct inode *dir, const char *, int len, kumode_t mode, struct inode **result);
};

struct inode *inode_get(struct super_block *, kino_t ino);
void inode_put(struct inode *);


static inline void inode_lock(struct inode *inode)
{
    mutex_lock(&inode->lock);
}

static inline void inode_unlock(struct inode *inode)
{
    mutex_unlock(&inode->lock);
}

#define using_inode(inode) \
    using_nocheck(inode_lock(inode), inode_unlock(inode))

#endif
