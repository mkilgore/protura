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
#include <protura/time.h>
#include <protura/errors.h>
#include <protura/list.h>
#include <protura/hlist.h>
#include <protura/atomic.h>
#include <protura/mutex.h>
#include <protura/bits.h>

#include <protura/fs/pipe.h>

struct inode_ops;
struct super_block;
struct file;
struct block_device;
struct char_device;

/*
 * Inode state transitions:
 *
 * ref>0             ref>=0                       ref=0
 *  0 >-------------> VALID >----------------> VALID+FREEING >------+
 *  V                  ^ V                           ^              |
 *  |                  | |                           |              |
 *  |           +------+ +-+                  ref=0  ^              |
 *  |           |          |             VALID+FREEING+DIRTY+SYNC   |
 *  |           |          |                         ^              |
 *  |           |          |                         |              |
 *  |           |   ref>=0 V                  ref=0  ^              |
 *  V ref>0     |     VALID+DIRTY >-------> VALID+FREEING+DIRTY     |
 * BAD          |          V                                        |
 *  V           |          |                                        |
 *  |           |    ref>0 V                                        |
 *  |           |  VALID+DIRTY+SYNC                                 |
 *  |           |          V                                        |
 *  |           |          |                                        |
 *  |           +----------+                                        |
 *  |                                                               V
 *  +-----------------------------------------------------------> free'd
 *
 * VALID: Inode's data is a correct representation of the inode. Invalid inodes
 *        have not yet been read from the disk or initialized beyond inode
 *        number and superblock.
 *
 * DIRTY: Contents of the inode different from the contents on disk.
 *
 * SYNC: The inode is currently being written to disk.
 *
 * FREEING: The inode is in the process of being removed from the hashtable and
 *          will eventually be free'd from memory, new references to it should
 *          not be handed out.
 *
 * BAD: Indicates an error during initialization (I/O error, inode did not
 *      exist, etc.). These inodes are immediately dropped.
 *
 * Notes:
 *  * INO_VALID is never unset. In certain situations this can be checked for without a lock.
 *
 *  * INO_BAD inodes never transition to INO_VALID, and thus are never returned from inode_get
 *      They are free'd immediately when their last reference is released.
 *      Newly created inodes can be marked bad via inode_mark_bad()
 *
 *  * During INO_SYNC, the inode is written to disk, then INO_SYNC+INO_DIRTY are cleared together
 *      During the syncing process a reference to the inode must be held the entire time.
 *
 *  * When INO_FREEING is set, references to that inode cannot be handed out
 *      and inode_get callers need to wait for the inode to be completely flushed
 *      to disk and free'd (This waiting happens on a separate queue from the one in the inode)
 *
 *  * The transition to INO_FREEING can only happen when ref=0
 *      Consequently, the initial state and INO_SYNC state can never transition
 *      to INO_FREEING, as they require at least one reference be present for the
 *      entire state.
 */
enum inode_flags {
    INO_VALID,
    INO_DIRTY,
    INO_FREEING,
    INO_SYNC,
    INO_BAD,
};

struct inode {
    ino_t ino;

    /* dev_no is the device-number this inode refers to, if it is a block or
     * char device. */
    dev_t dev_no;
    off_t size;
    mode_t mode;
    atomic32_t nlinks;
    uint32_t blocks;
    uint32_t block_size;

    time_t atime, mtime, ctime;

    uid_t uid;
    gid_t gid;

    spinlock_t flags_lock;
    flags_t flags;
    struct wait_queue flags_queue;
    atomic_t ref;

    /* Protects inode size/resize, along with writes to fields such as mode,
     * nlinks, blocks, a/m/c time */
    mutex_t lock;

    hlist_node_t hash_entry;
    list_node_t list_entry;

    list_node_t sync_entry;

    list_node_t sb_entry;
    list_node_t sb_dirty_entry;

    struct super_block *sb;
    struct inode_ops *ops;
    const struct file_ops *default_fops;

    struct block_device *bdev;
    struct char_device *cdev;

    struct pipe_info pipe_info;
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

    int (*create) (struct inode *dir, const char *name, size_t len, mode_t mode, struct inode **result);
    int (*mkdir) (struct inode *, const char *name, size_t len, mode_t mode);
    int (*link) (struct inode *dir, struct inode *old, const char *name, size_t len);
    int (*mknod) (struct inode *dir, const char *name, size_t len, mode_t mode, dev_t dev);

    int (*unlink) (struct inode *dir, struct inode *entity, const char *name, size_t len);
    int (*rmdir) (struct inode *dir, struct inode *entity, const char *name, size_t len);

    int (*rename) (struct inode *old_dir, const char *name, size_t len,
                   struct inode *new_dir, const char *new_name, size_t new_len);

    int (*symlink) (struct inode *dir, const char *symlink, size_t len, const char *symlink_target);
    int (*readlink) (struct inode *dir, char *, size_t buf_len);
    int (*follow_link) (struct inode *dir, struct inode *symlink, struct inode **result);
};

#define Pinode(i) (i)->sb->bdev->dev, (i)->ino
#define PRinode "%d:%d"

#define inode_has_truncate(inode) ((inode)->ops && (inode)->ops->truncate)
#define inode_has_lookup(inode) ((inode)->ops && (inode)->ops->lookup)
#define inode_has_bmap(inode) ((inode)->ops && (inode)->ops->bmap)
#define inode_has_bmap_alloc(inode) ((inode)->ops && (inode)->ops->bmap_alloc)
#define inode_has_link(inode) ((inode)->ops && (inode)->ops->link)
#define inode_has_unlink(inode) ((inode)->ops && (inode)->ops->unlink)
#define inode_has_create(inode) ((inode)->ops && (inode)->ops->create)
#define inode_has_mkdir(inode) ((inode)->ops && (inode)->ops->mkdir)
#define inode_has_mknod(inode) ((inode)->ops && (inode)->ops->mknod)
#define inode_has_rmdir(inode) ((inode)->ops && (inode)->ops->rmdir)
#define inode_has_rename(inode) ((inode)->ops && (inode)->ops->rename)
#define inode_has_symlink(inode) ((inode)->ops && (inode)->ops->symlink)
#define inode_has_readlink(inode) ((inode)->ops && (inode)->ops->readlink)
#define inode_has_follow_link(inode) ((inode)->ops && (inode)->ops->follow_link)

extern struct inode_ops inode_ops_null;

static inline void inode_init(struct inode *i)
{
    mutex_init(&i->lock);
    spinlock_init(&i->flags_lock);
    wait_queue_init(&i->flags_queue);
    atomic_init(&i->ref, 0);
    list_node_init(&i->list_entry);
    list_node_init(&i->sb_entry);
    list_node_init(&i->sb_dirty_entry);
    list_node_init(&i->sync_entry);
    hlist_node_init(&i->hash_entry);

    pipe_info_init(&i->pipe_info);
}

int inode_lookup_generic(struct inode *dir, const char *name, size_t len, struct inode **result);

static inline void inode_lock_read(struct inode *i)
{
    mutex_lock(&i->lock);
}

static inline void inode_unlock_read(struct inode *i)
{
    mutex_unlock(&i->lock);
}

static inline void inode_lock_write(struct inode *i)
{
    mutex_lock(&i->lock);
}

static inline void inode_unlock_write(struct inode *i)
{
    mutex_unlock(&i->lock);
}

static inline int inode_try_lock_write(struct inode *i)
{
    return mutex_try_lock(&i->lock);
}

#define using_inode_lock_read(inode) \
    using_mutex(&(inode)->lock)

#define using_inode_lock_write(inode) \
    using_mutex(&(inode)->lock)

#define using_inode(sb, ino, inode) \
    using((inode = inode_get(sb, ino)) != NULL, inode_put(inode))

/* If an inode was acquired from inode_get_invalid() and was returned without
 * INO_VALID set, then this will mark INO_VALID and wake up anybody waiting on
 * it to become valid. */
void inode_mark_valid(struct inode *);

/* If an inode was acquired from inode_get_invalid() and was returned without
 * INO_VALID set, this will mark the inode INO_BAD and handle freeing the inode
 * for you. After this is called the inode can no longer be used */
void inode_mark_bad(struct inode *);

void inode_put(struct inode *);

/* Creates a completely empty inode, not tied to any internal lists. Useful for
 * making inodes that will not be part of the global inode hashtable. */
struct inode *inode_create(struct super_block *);

/* Acquires a new reference to an inode. Note that when calling this you should
 * always already have a valid reference to the inode */
struct inode *inode_dup(struct inode *);

/* Returns the requested inode, or NULL if it does not exist.
 *
 * The returned inode will always have INO_VALID set, and never have INO_FREEING set. */
struct inode *inode_get(struct super_block *, ino_t ino);

/* Attempts to look up an inode. If it is not found, it creates a new inode
 * with the specified inode number, but does not call sb->read_inode. Instead,
 * the inode is returned without INO_VALID set and the caller is expected it
 * fill it out themselves and then call inode_make_valid() */
struct inode *inode_get_invalid(struct super_block *, ino_t ino);

/* Writes the specified inode to the disk. If wait=1, then the function doesn't
 * return until the contents have been wrriten to the disk */
int inode_write_to_disk(struct inode *, int wait);

/* If inode_write_to_disk() is called with wait=0, then this function allows
 * you to wait for the inode to be written to disk */
void inode_wait_for_write(struct inode *);

/* Syncs all inodes associatd with the superblock. If wait=1, the function
 * doesn't return until they are all written to disk */
void inode_sync(struct super_block *, int wait);

/* Sync every inode on every superblock */
void inode_sync_all(int wait);

/* Attempts to clear out every inode associated with a superblock. If inodes
 * are currently being free'd, it will wait for them to be free'd. If an inode
 * currently has a reference, then it will not be cleared.
 *
 * If all the inodes can be dropped, then it will also drop the root inode. It
 * is necessary to pass the root inode so that we know to expect an existing
 * reference to it. */
int inode_clear_super(struct super_block *, struct inode *root);

/* Attempts to clear out as many inode's as it can */
void inode_oom(void);

static inline void inode_set_dirty(struct inode *inode)
{
    using_spinlock(&inode->flags_lock)
        flag_set(&inode->flags, INO_DIRTY);
}

static inline void inode_inc_nlinks(struct inode *inode)
{
    atomic_inc(&inode->nlinks);
    inode_set_dirty(inode);
}

static inline void inode_dec_nlinks(struct inode *inode)
{
    atomic_dec(&inode->nlinks);
    inode_set_dirty(inode);
}

#endif
