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
#include <protura/list.h>
#include <protura/hlist.h>
#include <protura/semaphore.h>
#include <protura/dev.h>

struct block {
    /* The actual data for this block */
    char *data;

    /* 'block_size' is the fundimental size for this device. */
    size_t block_size;

    /* Location of the first block on the device, and the actual device it represents. */
    sector_t sector;
    dev_t dev;

    /* If this is set, then the contents of this block has been modified and
     * doesn't match the contents of the disk. */
    uint32_t dirty :1;

    /* If this isn't set, then that means the contents of this block aren't
     * correct, and need to be read from the disk. */
    uint32_t valid :1;

    /* To be able to modify this block, you have to acquire this lock */
    mutex_t block_mutex;

    /* If the block_mutex is currently locked, then this points to the task
     * that currently holds that lock */
    struct task *owner;

    struct list_node block_list_node;
    struct hlist_node cache;
    struct list_node block_lru_node;
};

static inline void block_lock(struct block *b)
{
    mutex_lock(&b->block_mutex);
    b->owner = cpu_get_local()->current;
}

static inline int block_try_lock(struct block *b)
{
    if (mutex_try_lock(&b->block_mutex) == SUCCESS) {
        b->owner = cpu_get_local()->current;
        return SUCCESS;
    }

    return 1;
}

static inline void block_unlock(struct block *b)
{
    b->owner = NULL;
    mutex_unlock(&b->block_mutex);
}

static inline void block_mark_dirty(struct block *b)
{
    b->dirty = 1;
}

static inline void block_mark_clean(struct block *b)
{
    b->dirty = 0;
}

static inline int block_waiting(struct block *b)
{
    return mutex_waiting(&b->block_mutex);
}

void block_init(struct block *);
void block_clear(struct block *);

struct block *bread(dev_t, sector_t);
void brelease(struct block *);

#define using_block(dev, sector, block) \
    using_nocheck(((block) = bread(dev, sector)), brelease(block))


void block_cache_init(void);
void block_cache_sync(void);

struct block_device;

struct block_device_ops {
    void (*sync_block) (struct block_device *, struct block *b);
};

struct block_device {
    const char *name;
    int major;

    size_t block_size;

    struct block_device_ops *ops;
};

enum {
    DEV_NONE = 0,
    DEV_IDE = 1,
};

void block_dev_init(void);

struct block_device *block_dev_get(dev_t device);

static inline void block_dev_sync_block(struct block_device *dev, struct block *b)
{
    if (!b->valid || b->dirty)
        (dev->ops->sync_block) (dev, b);
}

#endif
