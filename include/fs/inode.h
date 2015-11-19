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

#include <fs/inode_table.h>

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
    int nlinks;

    flags_t flags;

    mutex_t lock;

    atomic_t ref;
    struct hlist_node hash_entry;
    list_node_t list_entry;

    struct super_block *sb;
    struct inode_ops *ops;
    struct file_ops *default_fops;

    struct block_device *bdev;
    struct char_device *cdev;
};

enum inode_attributes_flags {
    INO_ATTR_SIZE,
};

struct inode_attributes {
    flags_t change;

    off_t size;
};

struct inode_ops {
    int (*lookup) (struct inode *dir, const char *name, size_t len, struct inode **result);
    int (*change_attrs) (struct inode *, struct inode_attributes *);
    sector_t (*bmap) (struct inode *, sector_t);
};

#define inode_has_change_attrs(inode) ((inode)->ops && (inode)->ops->change_attrs)
#define inode_has_lookup(inode) ((inode)->ops && (inode)->ops->lookup)
#define inode_has_bmap(inode) ((inode)->ops && (inode)->ops->bmap)

#define inode_is_valid(inode) bit_test(&(inode)->flags, INO_VALID)
#define inode_is_dirty(inode) bit_test(&(inode)->flags, INO_DIRTY)

#define inode_set_valid(inode) bit_set(&(inode)->flags, INO_VALID)
#define inode_set_dirty(inode) bit_set(&(inode)->flags, INO_DIRTY)

#define inode_clear_valid(inode) bit_clear(&(inode)->flags, INO_VALID)
#define inode_clear_dirty(inode) bit_clear(&(inode)->flags, INO_DIRTY)

extern struct inode_ops inode_ops_null;

static inline void inode_init(struct inode *i)
{
    mutex_init(&i->lock);
    atomic_init(&i->ref, 0);
    list_node_init(&i->list_entry);
}

int inode_lookup_generic(struct inode *dir, const char *name, size_t len, struct inode **result);

static inline void inode_lock_read(struct inode *i)
{
    kp(KP_LOCK, "inode %d:%d: Locking read\n", i->ino, i->dev);
    mutex_lock(&i->lock);
    kp(KP_LOCK, "inode %d:%d: Locked read\n", i->ino, i->dev);
}

static inline void inode_unlock_read(struct inode *i)
{
    kp(KP_LOCK, "inode %d:%d: Unlocking read\n", i->ino, i->dev);
    mutex_unlock(&i->lock);
    kp(KP_LOCK, "inode %d:%d: Unlocked read\n", i->ino, i->dev);
}

static inline void inode_lock_write(struct inode *i)
{
    kp(KP_LOCK, "inode %d:%d: Locking write\n", i->ino, i->dev);
    mutex_lock(&i->lock);
    kp(KP_LOCK, "inode %d:%d: Locked write\n", i->ino, i->dev);
}

static inline void inode_unlock_write(struct inode *i)
{
    kp(KP_LOCK, "inode %d:%d: Unlocking write\n", i->ino, i->dev);
    mutex_unlock(&i->lock);
    kp(KP_LOCK, "inode %d:%d: Unlocked write\n", i->ino, i->dev);
}

static inline int inode_try_lock_write(struct inode *i)
{
    kp(KP_LOCK, "inode %d:%d: Attempting Locking write\n", i->ino, i->dev);
    if (mutex_try_lock(&i->lock)) {
        kp(KP_LOCK, "inode %d:%d: Locked write\n", i->ino, i->dev);
        return 1;
    }
    return 0;
}

#define using_inode_lock_read(inode) \
    using_nocheck(inode_lock_read(inode), inode_unlock_read(inode))

#define using_inode_lock_write(inode) \
    using_nocheck(inode_lock_write(inode), inode_unlock_write(inode))

#define using_inode(sb, ino, inode) \
    using((inode = inode_get(sb, ino)) != NULL, inode_put(inode))

void inode_cache_flush(void);

#endif
