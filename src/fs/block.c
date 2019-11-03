/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/list.h>
#include <protura/scheduler.h>
#include <protura/mm/kmalloc.h>
#include <protura/drivers/ide.h>
#include <protura/crc.h>

#include <arch/spinlock.h>
#include <arch/task.h>
#include <arch/cpu.h>
#include <protura/fs/block.h>

#define BLOCK_HASH_TABLE_SIZE CONFIG_BLOCK_HASH_TABLE_SIZE

static struct {
    mutex_t lock;

    /* List of cached blocks ready to be checked-out.
     *
     * Note: Some of the cached blocks may be currently locked by another
     * process. */
    int cache_count;
    struct hlist_head cache[BLOCK_HASH_TABLE_SIZE];
    list_head_t lru;
} block_cache = {
    .lock = MUTEX_INIT(block_cache.lock, "Block-cache-lock"),
    .cache_count = 0,
    .cache = { { NULL }, },
    .lru = LIST_HEAD_INIT(block_cache.lru),
};

static inline int block_hash(dev_t device, sector_t sector)
{
    int hash;
    hash = ((DEV_MAJOR(device) + DEV_MINOR(device)) ^ (sector)) % BLOCK_HASH_TABLE_SIZE;
    return hash;
}

static void __block_uncache(struct block *b)
{
    /* Remove this block from the cache */
    hlist_del(&b->cache);
    list_del(&b->block_lru_node);
    list_del(&b->bdev_blocks_entry);

    if (list_node_is_in_list(&b->block_sync_node)) {
        list_del(&b->block_sync_node);
        atomic_dec(&b->refs);
    }

    block_cache.cache_count--;
}

static void block_delete(struct block *b)
{
    if (b->block_size == PG_SIZE)
        pfree_va(b->data, 0);
    else
        kfree(b->data);

    kfree(b);
}

static struct block *block_new(void)
{
    struct block *b = kzalloc(sizeof(*b), PAL_KERNEL);

    mutex_init(&b->block_mutex);
    list_node_init(&b->block_list_node);
    list_node_init(&b->block_lru_node);
    list_node_init(&b->bdev_blocks_entry);
    list_node_init(&b->block_sync_node);

    wait_queue_init(&b->queue);

    atomic_init(&b->refs, 0);

    return b;
}

void __block_cache_shrink(void)
{
    int i = 0;
    struct block *b, *next, *first;

    /* We loop over the list backwards. We start at the 'first' entry so that
     * when we call 'list_prev_entry()' we get the last entry in the list. */
    first = list_first_entry(&block_cache.lru, struct block, block_lru_node);

    next = list_last_entry(&block_cache.lru, struct block, block_lru_node);

    for (i = 0; i < CONFIG_BLOCK_CACHE_SHRINK_COUNT; i++) {
        b = next;
        next = list_prev_entry(b, block_lru_node);

        /* If we hit the first entry, then we exit the loop.
         *
         * Note: If this happens there's probably a error somewhere else in the
         * kernel causing lots of blocks to not be unlocked/released, because a
         * large majority of the blocks in the cache are currently locked. */
        if (b == first)
            break;

        if (block_try_lock(b) != SUCCESS)
            continue;

        if (atomic_get(&b->refs) != 0)
            continue;

        /* Because we're holding the block_cache.lock, it's impossible for
         * anybody to acquire a new reference to this block Sync it if
         * necessary, and drop it */

        (b->bdev->ops->sync_block) (b->bdev, b);

        /* Remove this block from the cache */
        __block_uncache(b);

        block_delete(b);
    }
}

static struct block *__bread(dev_t device, struct block_device *bdev, size_t block_size, sector_t sector)
{
    struct block *b = NULL;
    int hash = block_hash(device, sector);

    hlist_foreach_entry(block_cache.cache + hash, b, cache)
        if (b->dev == device && b->sector == sector)
            break;

    if (b)
        goto inc_and_return;

    /* We do the shrink *before* we allocate a new block if it is necessary.
     * This is to ensure the shrink can't remove the block we're about to add
     * from the cache. */
    if (block_cache.cache_count >= CONFIG_BLOCK_CACHE_MAX_SIZE)
        __block_cache_shrink();

    b = block_new();
    b->block_size = block_size;

    if (block_size != PG_SIZE)
        b->data = kzalloc(block_size, PAL_KERNEL);
    else
        b->data = palloc_va(0, PAL_KERNEL);

    b->sector = sector;
    b->dev = device;
    b->bdev = bdev;

    /* Insert our new block into the cache */
    hlist_add(block_cache.cache + hash, &b->cache);
    block_cache.cache_count++;

    list_add(&bdev->blocks, &b->bdev_blocks_entry);

  inc_and_return:
    atomic_inc(&b->refs);
    return b;
}

struct block *bread(dev_t device, sector_t sector)
{
    struct block *b;
    struct block_device *dev = block_dev_get(device);

    if (!dev)
        return NULL;

    using_mutex(&block_cache.lock) {
        b = __bread(device, dev, block_dev_get_block_size(device), sector);

        if (list_node_is_in_list(&b->block_lru_node))
            list_del(&b->block_lru_node);
        list_add(&block_cache.lru, &b->block_lru_node);
    }

    /* We can't attempt to lock the block while we have block_cache.lock, or
     * else we might sleep with the block_cache still locked, which will bring
     * any block-IO to a halt until we're woke-up.
     *
     * Note that even if we sleep, this block will always be valid when we're
     * woke-up because a block won't be removed from the cache unless it has an
     * empty wait_queue. */
    block_lock(b);

    if (!flag_test(&b->flags, BLOCK_VALID))
        block_dev_sync_block(b->bdev, b);

    return b;
}

void brelease(struct block *b)
{
    block_unlock(b);
    atomic_dec(&b->refs);
}

void block_dev_clear(dev_t dev)
{
    struct block_device *bdev = block_dev_get(dev);
    struct block *b, *nxt;

    using_mutex(&block_cache.lock) {
        list_foreach_entry_safe(&bdev->blocks, b, nxt, bdev_blocks_entry) {
            if (dev != DEV_NONE && b->dev != dev)
                continue;

            if (atomic_get(&b->refs) != 0)
                panic("EXT2: Reference to Block %d:%d held when block_dev_clear was called!!!!\n", b->dev, b->sector);

            block_dev_sync_block(bdev, b);
            __block_uncache(b);
            block_delete(b);
        }
    }
}

void block_dev_sync(struct block_device *bdev, dev_t dev, int wait)
{
    /* we cannot wait on a block while holding the lock block_cache.lock, so to avoid this issue */
    list_head_t sync_list = LIST_HEAD_INIT(sync_list);
    struct block *b;

    using_mutex(&block_cache.lock) {
        list_foreach_entry(&bdev->blocks, b, bdev_blocks_entry) {
            if (dev != DEV_NONE && b->dev != dev)
                continue;

            if (!mutex_try_lock(&b->block_mutex)) {
                if (wait) {
                    /* We have to be a little careful, multiple syncs can
                     * produce a race condition due to multiple `sync_list`s
                     * wanting the same block. We need to take a reference to
                     * all of the blocks to ensure they won't be removed from
                     * the cache, but we also need to make sure multiple syncs
                     * don't result in multiple references that won't be
                     * removed. */
                    if (list_node_is_in_list(&b->block_sync_node))
                        list_del(&b->block_sync_node);
                    else
                        atomic_inc(&b->refs);

                    list_add_tail(&sync_list, &b->block_sync_node);
                }

                continue;
            }

            if (flag_test(&b->flags, BLOCK_DIRTY))
                block_dev_sync_block(b->bdev, b);

            mutex_unlock(&b->block_mutex);
        }

        while (!list_empty(&sync_list)) {
            b = list_take_first(&sync_list, struct block, block_sync_node);

            not_using_mutex(&block_cache.lock) {
                block_lock(b);

                if (flag_test(&b->flags, BLOCK_DIRTY))
                    block_dev_sync_block(b->bdev, b);

                block_unlock(b);
                atomic_dec(&b->refs);
            }
        }
    }
}
