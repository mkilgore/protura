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

#include <protura/fs/inode_table.h>

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

    /* sb_dev is the block-device attached to the super-block.
     *
     * dev_no is the device-number this inode refers to, if it is a block or
     * char device. */
    dev_t sb_dev;
    dev_t dev_no;
    off_t size;
    mode_t mode;
    atomic32_t nlinks;

    flags_t flags;

    mutex_t lock;

    atomic_t ref;
    struct hlist_node hash_entry;
    list_node_t list_entry;

    list_node_t sb_dirty_entry;

    struct super_block *sb;
    struct inode_ops *ops;
    struct file_ops *default_fops;

    struct block_device *bdev;
    struct char_device *cdev;
};

enum inode_attributes_flags {
    INO_ATTR_SIZE,
};

struct inode_ops {
    /* Look-up the string 'name' of length 'len' in the provided directory
     * inode. If found, then the coresponding inode is returned in 'result' */
    int (*lookup) (struct inode *dir, const char *name, size_t len, struct inode **result);

    /* Change the attributes of the provided inode using the provided new
     * 'struct inode_attributes' object */
    int (*truncate) (struct inode *, off_t size);

    /* Standard bmap - Map a sector from the provided inode to a sector on the
     * underlying block-device */
    sector_t (*bmap) (struct inode *, sector_t);

    /* Allocate bmap - Same as bmap, but if the sector does not have an
     * underlying sector on the block-device, a new one is given to the provide
     * inode and returned. - Generally used for writing to files, as sparse
     * files may not have blocks mapped in every inode sector. */
    sector_t (*bmap_alloc) (struct inode *, sector_t);

    int (*mkdir) (struct inode *, const char *name, size_t len, ino_t ino, mode_t mode);
    int (*link) (struct inode *dir, struct inode *old, const char *name, size_t len);
};

#define Pinode(i) (i)->sb_dev, (i)->ino
#define PRinode "%d:%d"

#define inode_has_truncate(inode) ((inode)->ops && (inode)->ops->truncate)
#define inode_has_lookup(inode) ((inode)->ops && (inode)->ops->lookup)
#define inode_has_bmap(inode) ((inode)->ops && (inode)->ops->bmap)
#define inode_has_bmap_alloc(inode) ((inode)->ops && (inode)->ops->bmap_alloc)
#define inode_has_link(inode) ((inode)->ops && (inode)->ops->link)

#define inode_is_valid(inode) bit_test(&(inode)->flags, INO_VALID)
#define inode_is_dirty(inode) bit_test(&(inode)->flags, INO_DIRTY)

#define inode_set_valid(inode) bit_set(&(inode)->flags, INO_VALID)
#define inode_set_dirty(inode) \
    do { \
        using_mutex(&(inode)->sb->dirty_inodes_lock) { \
            if (!bit_test(&(inode)->flags, INO_DIRTY)) { \
                bit_set(&(inode)->flags, INO_DIRTY); \
                kp(KP_TRACE, "Adding "PRinode": dirty\n", Pinode(inode)); \
                list_add(&(inode)->sb->dirty_inodes, &(inode)->sb_dirty_entry); \
            } \
        } \
    } while (0)

#define inode_clear_valid(inode) bit_clear(&(inode)->flags, INO_VALID)
#define inode_clear_dirty(inode) bit_clear(&(inode)->flags, INO_DIRTY)

extern struct inode_ops inode_ops_null;

static inline void inode_init(struct inode *i)
{
    mutex_init(&i->lock);
    atomic_init(&i->ref, 0);
    list_node_init(&i->list_entry);
    list_node_init(&i->sb_dirty_entry);
}

int inode_lookup_generic(struct inode *dir, const char *name, size_t len, struct inode **result);

static inline void inode_lock_read(struct inode *i)
{
    kp(KP_LOCK, "inode "PRinode": Locking read\n", Pinode(i));
    mutex_lock(&i->lock);
    kp(KP_LOCK, "inode "PRinode": Locked read\n", Pinode(i));
}

static inline void inode_unlock_read(struct inode *i)
{
    kp(KP_LOCK, "inode "PRinode": Unlocking read\n", Pinode(i));
    mutex_unlock(&i->lock);
    kp(KP_LOCK, "inode "PRinode": Unlocked read\n", Pinode(i));
}

static inline void inode_lock_write(struct inode *i)
{
    kp(KP_LOCK, "inode "PRinode": Locking write\n", Pinode(i));
    mutex_lock(&i->lock);
    kp(KP_LOCK, "inode "PRinode": Locked write\n", Pinode(i));
}

static inline void inode_unlock_write(struct inode *i)
{
    kp(KP_LOCK, "inode "PRinode": Unlocking write\n", Pinode(i));
    mutex_unlock(&i->lock);
    kp(KP_LOCK, "inode "PRinode": Unlocked write\n", Pinode(i));
}

static inline int inode_try_lock_write(struct inode *i)
{
    kp(KP_LOCK, "inode "PRinode": Attempting Locking write\n", Pinode(i));
    if (mutex_try_lock(&i->lock)) {
        kp(KP_LOCK, "inode "PRinode": Locked write\n", Pinode(i));
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
