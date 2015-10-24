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
#include <protura/mutex.h>
#include <protura/bits.h>

struct inode_ops;
struct super_block;
struct file;
struct block_device;
struct char_device;

enum inode_flags {
    INO_VALID, /* Inode's data is a valid version of the data */
    INO_DIRTY, /* Inode's data is not the same as the disk's version */
};

struct inode {
    ino_t ino;
    dev_t dev;
    off_t size;
    mode_t mode;

    flags_t flags;

    mutex_t lock;

    atomic_t ref;
    struct hlist_node hash_entry;
    struct list_node list_entry;

    struct super_block *sb;
    struct inode_ops *ops;
    struct file_ops *default_fops;

    struct block_device *bdev;
    struct char_device *cdev;
};

struct inode_ops {
    int (*truncate) (struct inode *, off_t len);
    int (*lookup) (struct inode *dir, const char *name, size_t len, struct inode **result);
    sector_t (*bmap) (struct inode *, sector_t);
};

#define inode_is_valid(inode) bit_test(&(inode)->flags, INO_VALID)
#define inode_is_dirty(inode) bit_test(&(inode)->flags, INO_DIRTY)

#define inode_set_valid(inode) bit_set(&(inode)->flags, INO_VALID)
#define inode_set_dirty(inode) bit_set(&(inode)->flags, INO_DIRTY)

#define inode_clear_valid(inode) bit_clear(&(inode)->flags, INO_VALID)
#define inode_clear_dirty(inode) bit_clear(&(inode)->flags, INO_DIRTY)

static inline void inode_init(struct inode *i)
{
    mutex_init(&i->lock);
    atomic_init(&i->ref, 0);
}

int inode_lookup_generic(struct inode *dir, const char *name, size_t len, struct inode **result);

#define using_inode_lock_read(inode) \
    using_mutex(&(inode)->lock)

#define using_inode_lock_write(inode) \
    using_mutex(&(inode)->lock)

#define inode_lock(inode) \
    mutex_lock(&(inode)->lock)

#define inode_unlock(inode) \
    mutex_unlock(&(inode)->lock)

#define inode_try_lock(inode) \
    mutex_try_lock(&(inode)->lock)

#define using_inode(sb, ino, inode) \
    using((inode = inode_get(sb, ino)) != NULL, inode_put(inode))

#define inode_has_truncate(inode) ((inode)->ops && (inode)->ops->truncate)
#define inode_has_lookup(inode) ((inode)->ops && (inode)->ops->lookup)
#define inode_has_bmap(inode) ((inode)->ops && (inode)->ops->bmap)

void inode_cache_flush(void);

#endif
