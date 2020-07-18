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
    .lock = MUTEX_INIT(block_cache.lock),
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

    spinlock_init(&b->flags_lock);
    wait_queue_init(&b->flags_queue);

    list_node_init(&b->block_list_node);
    list_node_init(&b->block_lru_node);
    list_node_init(&b->bdev_blocks_entry);
    list_node_init(&b->block_sync_node);

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

        if (atomic_get(&b->refs) != 0) {
            block_unlock(b);
            continue;
        }

        if (!flag_test(&b->flags, BLOCK_DIRTY)) {
            block_unlock(b);
            continue;
        }

        /* Remove this block from the cache */
        __block_uncache(b);

        block_delete(b);
    }
}

void block_wait_for_sync(struct block *b)
{
    using_spinlock(&b->flags_lock)
        wait_queue_event_spinlock(&b->flags_queue, !flag_test(&b->flags, BLOCK_LOCKED), &b->flags_lock);
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

    /* We can check this without the lock because BLOCK_VALID is never removed
     * once set, and syncing an extra time isn't a big deal. */
    if (!flag_test(&b->flags, BLOCK_VALID)) {
        block_lock(b);

        /* We check again just to avoid doing multiple syncs if we don't need
         * too - the block could have become valid while we waited to lock it */
        block_dev_sync_block_async(b->bdev, b);

        /* Wait for syncing */
        block_wait_for_sync(b);
    }

    return b;
}

void brelease(struct block *b)
{
    atomic_dec(&b->refs);
}

/* Protects the 'b->block_sync_node' entries.
 * Ordering:
 *   sync_lock
 *     block_cache.lock
 */
static mutex_t sync_lock = MUTEX_INIT(sync_lock);

void block_dev_clear(dev_t dev)
{
    struct block_device *bdev = block_dev_get(dev);
    struct block *b;

    block_dev_sync(bdev, dev, 1);

    using_mutex(&block_cache.lock) {
        list_foreach_take_entry(&bdev->blocks, b, bdev_blocks_entry) {
            if (dev != DEV_NONE && b->dev != dev)
                continue;

            if (block_try_lock(b) != SUCCESS || atomic_get(&b->refs) != 0 || flag_test(&b->flags, BLOCK_DIRTY)) {
                kp(KP_WARNING, "BLOCK: Reference to Block %d:%d held when block_dev_clear was called!!!!\n", b->dev, b->sector);
                continue;
            }

            __block_uncache(b);
            block_delete(b);
        }
    }
}

void block_dev_sync(struct block_device *bdev, dev_t dev, int wait)
{
    list_head_t sync_list = LIST_HEAD_INIT(sync_list);
    struct block *b;

    using_mutex(&sync_lock) {
        using_mutex(&block_cache.lock) {
            /* Note that this looping is safe because we always ensure we have a
             * reference to a block before dropping the `block_cache.lock`, which
             * means the current node will always be present when we're done */
            list_foreach_entry(&bdev->blocks, b, bdev_blocks_entry) {
                if (dev != DEV_NONE && b->dev != dev)
                    continue;

                atomic_inc(&b->refs);
                not_using_mutex(&block_cache.lock) {
                    block_lock(b);

                    if (flag_test(&b->flags, BLOCK_VALID) && flag_test(&b->flags, BLOCK_DIRTY)) {
                        block_dev_sync_block_async(b->bdev, b);
                        if (wait) {
                            atomic_inc(&b->refs);
                            list_add_tail(&sync_list, &b->block_sync_node);
                        }
                    } else {
                        block_unlock(b);
                    }
                }
                atomic_dec(&b->refs);
            }
        }

        if (!wait)
            return;

        while (!list_empty(&sync_list)) {
            b = list_take_first(&sync_list, struct block, block_sync_node);

            /* Wait for block to be synced, and then drop reference */
            block_wait_for_sync(b);
            atomic_dec(&b->refs);
        }
    }
}

void block_sync_all(int wait)
{
    list_head_t sync_list = LIST_HEAD_INIT(sync_list);
    struct block *b;

    using_mutex(&sync_lock) {
        using_mutex(&block_cache.lock) {
            int hash;

            /* Note that this looping is safe because we always ensure we have a
             * reference to a block before dropping the `block_cache.lock`, which
             * means the current node will always be present when we're done */
            for (hash = 0; hash < BLOCK_HASH_TABLE_SIZE; hash++) {
                hlist_foreach_entry(block_cache.cache + hash, b, cache) {

                    atomic_inc(&b->refs);
                    not_using_mutex(&block_cache.lock) {
                        block_lock(b);

                        if (flag_test(&b->flags, BLOCK_VALID) && flag_test(&b->flags, BLOCK_DIRTY)) {
                            block_dev_sync_block_async(b->bdev, b);
                            if (wait) {
                                atomic_inc(&b->refs);
                                list_add_tail(&sync_list, &b->block_sync_node);
                            }
                        } else {
                            block_unlock(b);
                        }
                    }
                    atomic_dec(&b->refs);
                }
            }
        }

        if (!wait)
            return;

        while (!list_empty(&sync_list)) {
            b = list_take_first(&sync_list, struct block, block_sync_node);

            /* Wait for block to be synced, and then drop reference */
            block_wait_for_sync(b);
            atomic_dec(&b->refs);
        }
    }
}
