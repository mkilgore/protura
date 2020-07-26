/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_BLOCK_H
#define INCLUDE_FS_BLOCK_H

#include <protura/types.h>
#include <protura/stddef.h>
#include <protura/debug.h>
#include <protura/scheduler.h>
#include <protura/wait.h>
#include <protura/list.h>
#include <protura/hlist.h>
#include <protura/mutex.h>
#include <protura/atomic.h>
#include <protura/dev.h>
#include <protura/crc.h>

struct block_device;

/*
 * Block state transitions:
 *
 *    0            free'd
 *    V              ^
 *    |              |
 *    V              ^
 *  LOCKED >-----> VALID <----+  VALID+DIRTY
 *                  ^ V       |    V   ^
 *                  | |       |    |   |
 *                  ^ V       |    |   |
 *             VALID+LOCKED   |    |   |
 *                   V        |    |   |
 *                   |        |    |   |
 *                   V        ^    V   |
 *                VALID+DIRTY+LOCKED >-+
 *
 * VALID: Block's data is an accurate representation of what should be on the disk.
 *
 * DIRTY: Block's data has been modified from what was on disk.
 *
 * LOCKED: Block's data is currently being accessed (To read it, modify it,
 *         sync it, etc.), nobody else can access the data until it is unlocked.
 *
 * Notes:
 *  * The state is protected by the flags_lock
 *
 *  * When the block is LOCKED, refs > 0
 *
 *  * Only the person who has locked the block can set or clear VALID and
 *    DIRTY. Consequently, the person who locked the block does not need to take
 *    flags_lock to *view* these bits. They still need to take it to modify them.
 */
enum {
    BLOCK_DIRTY,
    BLOCK_VALID,
    BLOCK_LOCKED,
};

struct block {
    atomic_t refs;

    /* The actual data for this block */
    char *data;

    /* 'block_size' is the size of the data for this block. */
    size_t block_size;

    /* Location of the first block on the device, and the actual device it represents. */
    sector_t sector;
    dev_t dev;

    struct block_device *bdev;
    list_node_t bdev_blocks_entry;

    spinlock_t flags_lock;
    flags_t flags;
    struct wait_queue flags_queue;

    list_node_t block_sync_node;

    list_node_t block_list_node;
    struct hlist_node cache;
    list_node_t block_lru_node;
};

static inline void block_mark_dirty(struct block *b)
{
    using_spinlock(&b->flags_lock)
        flag_set(&b->flags, BLOCK_DIRTY);
}

static inline void block_mark_clean(struct block *b)
{
    using_spinlock(&b->flags_lock)
        flag_clear(&b->flags, BLOCK_DIRTY);
}

static inline void block_mark_synced(struct block *b)
{
    using_spinlock(&b->flags_lock) {
        flag_set(&b->flags, BLOCK_VALID);
        flag_clear(&b->flags, BLOCK_DIRTY);
    }
}

static inline int block_waiting(struct block *b)
{
    return wait_queue_waiting(&b->flags_queue);
}

static inline void block_lock(struct block *b)
{
    using_spinlock(&b->flags_lock) {
        wait_queue_event_spinlock(&b->flags_queue, !flag_test(&b->flags, BLOCK_LOCKED), &b->flags_lock);

        flag_set(&b->flags, BLOCK_LOCKED);
    }
}

static inline int block_try_lock(struct block *b)
{
    using_spinlock(&b->flags_lock) {
        if (!flag_test(&b->flags, BLOCK_LOCKED)) {
            flag_set(&b->flags, BLOCK_LOCKED);
            return SUCCESS;
        }
    }

    return 1;
}

static inline void block_unlock(struct block *b)
{
    using_spinlock(&b->flags_lock) {
        flag_clear(&b->flags, BLOCK_LOCKED);
        wait_queue_wake(&b->flags_queue);
    }
}

struct block *block_get(dev_t, sector_t);
void block_put(struct block *);
void block_wait_for_sync(struct block *);

static inline struct block *block_dup(struct block *b)
{
    atomic_inc(&b->refs);
    return b;
}

static inline struct block *block_getlock(dev_t dev, sector_t sec)
{
    struct block *b = block_get(dev, sec);
    block_lock(b);
    return b;
}

static inline void block_unlockput(struct block *b)
{
    block_unlock(b);
    block_put(b);
}

#define using_block(dev, sector, block) \
    using_nocheck(((block) = block_get(dev, sector)), (block_put(block)))

#define using_block_locked(dev, sector, block) \
    using_nocheck(((block) = block_getlock(dev, sector)), (block_unlockput(block)))

void block_cache_init(void);

struct file_ops;

struct partition {
    list_node_t part_entry;
    sector_t start;
    size_t block_size;
    size_t device_size;
};

static inline void partition_init(struct partition *part)
{
    *part = (struct partition){ .part_entry = LIST_NODE_INIT((*part).part_entry) };
}

struct block_device_ops {
    void (*sync_block) (struct block_device *, struct block *b);
};

enum block_device_flags{
    BLOCK_DEV_EXISTS,
};

struct block_device {
    const char *name;
    int major;
    flags_t flags;

    list_head_t blocks;

    size_t block_size;
    size_t device_size;

    struct partition *partitions;
    int partition_count;

    void *priv;

    struct block_device_ops *ops;
    struct file_ops *fops;
};

extern struct file_ops block_dev_file_ops_generic;

int block_dev_file_open_generic(struct inode *dev, struct file *filp);
int block_dev_file_close_generic(struct file *);

enum {
    BLOCK_DEV_NONE = 0,
    BLOCK_DEV_IDE_MASTER = 1,
    BLOCK_DEV_IDE_SLAVE = 2,
    BLOCK_DEV_ANON = 3,
};

void block_dev_init(void);

struct block_device *block_dev_get(dev_t device);

int block_dev_set_block_size(dev_t device, size_t size);
size_t block_dev_get_block_size(dev_t device);
size_t block_dev_get_device_size(dev_t device);

void block_dev_clear(dev_t dev);
void block_dev_sync(struct block_device *, dev_t, int wait);
void block_sync_all(int wait);

static inline void block_submit(struct block *b)
{
    kassert(flag_test(&b->flags, BLOCK_LOCKED), "block_submit() called with an unlocked block!");

    (b->bdev->ops->sync_block) (b->bdev, b);
}

dev_t block_dev_anon_get(void);
void block_dev_anon_put(dev_t);

#endif
