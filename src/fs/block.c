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
    spinlock_t lock;

    /* List of cached blocks ready to be checked-out.
     *
     * Note: Some of the cached blocks may be currently locked by another
     * process. */
    int cache_count;
    struct hlist_head cache[BLOCK_HASH_TABLE_SIZE];
    list_head_t lru;
} block_cache = {
    .lock = SPINLOCK_INIT(),
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

static void __block_cache(struct block *b)
{
    int hash = block_hash(b->dev, b->sector);
    hlist_add(block_cache.cache + hash, &b->cache);
    block_cache.cache_count++;

    list_add(&b->bdev->blocks, &b->bdev_blocks_entry);
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

        /* Don't need the spinlock, there's no existing references */
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

static struct block *__find_block(dev_t device, sector_t sector)
{
    struct block *b;
    int hash = block_hash(device, sector);

    hlist_foreach_entry(block_cache.cache + hash, b, cache)
        if (b->dev == device && b->sector == sector)
            return b;

    return NULL;
}

/* This function returns the `struct block *` for the given device and sector,
 * but does not sync it, so it may be completely fresh and not actually read
 * off the disk. */
static struct block *block_get_nosync(dev_t device, struct block_device *bdev, size_t block_size, sector_t sector)
{
    struct block *b = NULL, *new = NULL;

    spinlock_acquire(&block_cache.lock);

    b = __find_block(device, sector);
    if (b)
        goto inc_and_return;

    /* We do the shrink *before* we allocate a new block if it is necessary.
     * This is to ensure the shrink can't remove the block we're about to add
     * from the cache. */
    if (block_cache.cache_count >= CONFIG_BLOCK_CACHE_MAX_SIZE)
        __block_cache_shrink();

    spinlock_release(&block_cache.lock);

    new = block_new();
    new->block_size = block_size;

    if (block_size != PG_SIZE)
        new->data = kzalloc(block_size, PAL_KERNEL);
    else
        new->data = palloc_va(0, PAL_KERNEL);

    new->sector = sector;
    new->dev = device;
    new->bdev = bdev;

    spinlock_acquire(&block_cache.lock);

    /* We had to drop the lock because allocating the memory may sleep. If
     * there is a race and a second allocation happens for the same block then
     * the block we just made might already be in the hash list.  In that
     * situation we simply delete the one we just made and return the existing
     * one. */
    b = __find_block(device, sector);

    if (b) {
        block_delete(new);
        goto inc_and_return;
    }

    /* Insert our new block into the cache */
    __block_cache(new);
    b = new;

  inc_and_return:
    atomic_inc(&b->refs);

    /* Refresh the LRU entry for this block */
    if (list_node_is_in_list(&b->block_lru_node))
        list_del(&b->block_lru_node);
    list_add(&block_cache.lru, &b->block_lru_node);

    spinlock_release(&block_cache.lock);
    return b;
}

struct block *block_get(dev_t device, sector_t sector)
{
    struct block *b;
    struct block_device *dev = block_dev_get(device);

    if (!dev)
        return NULL;

    b = block_get_nosync(device, dev, block_dev_get_block_size(device), sector);
    if (!b)
        return NULL;

    /* We can check this without the lock because BLOCK_VALID is never removed
     * once set, and syncing an extra time isn't a big deal. */
    if (!flag_test(&b->flags, BLOCK_VALID)) {
        int should_sync = 0, should_wait = 0;

        /* If the block is already LOCKED, then it will become VALID once's
         * that done and we don't need to submit it outselves */
        using_spinlock(&b->flags_lock) {
            if (flag_test(&b->flags, BLOCK_LOCKED))
                should_wait = 1;
            else if (!flag_test(&b->flags, BLOCK_VALID))
                should_sync = 1;
        }

        if (should_sync) {
            block_lock(b);
            block_submit(b);
        }

        if (should_wait || should_sync)
            block_wait_for_sync(b);
    }

    return b;
}

void block_put(struct block *b)
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

    using_spinlock(&block_cache.lock) {
        list_foreach_take_entry(&bdev->blocks, b, bdev_blocks_entry) {
            if (dev != DEV_NONE && b->dev != dev)
                continue;

            if (block_try_lock(b) != SUCCESS || atomic_get(&b->refs) != 0) {
                kp(KP_WARNING, "BLOCK: Reference to Block %d:%d held when block_dev_clear was called!!!!\n", b->dev, b->sector);
                continue;
            }

            if (flag_test(&b->flags, BLOCK_DIRTY)) {
                kp(KP_WARNING, "BLOCK: Block %d:%d was still dirty when block_dev_clear was called!!!!\n", b->dev, b->sector);
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
    struct block *b, *next;

    using_mutex(&sync_lock) {
        using_spinlock(&block_cache.lock) {
            list_foreach_entry(&bdev->blocks, b, bdev_blocks_entry) {
                if (dev != DEV_NONE && b->dev != dev)
                    continue;

                using_spinlock(&b->flags_lock) {
                    if (flag_test(&b->flags, BLOCK_VALID) && flag_test(&b->flags, BLOCK_DIRTY)) {
                        atomic_inc(&b->refs);
                        list_add_tail(&sync_list, &b->block_sync_node);
                    }
                }
            }
        }

        /* It's better to to simply hold the block_cache.lock spinlock the
         * whole time and then submit all the blocks down here.
         *
         * Safe is used because if we're not waiting we need to drop the reference here too. */
        list_foreach_entry_safe(&sync_list, b, next, block_sync_node) {
            block_lock(b);
            block_submit(b);

            if (!wait)
                block_put(b);
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
    struct block *b, *next;

    using_mutex(&sync_lock) {
        using_spinlock(&block_cache.lock) {
            int hash;

            for (hash = 0; hash < BLOCK_HASH_TABLE_SIZE; hash++) {
                hlist_foreach_entry(block_cache.cache + hash, b, cache) {

                    using_spinlock(&b->flags_lock) {
                        if (flag_test(&b->flags, BLOCK_VALID) && flag_test(&b->flags, BLOCK_DIRTY)) {
                            atomic_inc(&b->refs);
                            list_add_tail(&sync_list, &b->block_sync_node);
                        }
                    }
                }
            }
        }

        /* It's better to to simply hold the block_cache.lock spinlock the
         * whole time and then submit all the blocks down here.
         *
         * Safe is used because if we're not waiting we need to drop the reference here too. */
        list_foreach_entry_safe(&sync_list, b, next, block_sync_node) {
            block_lock(b);

            if (!flag_test(&b->flags, BLOCK_VALID) || !flag_test(&b->flags, BLOCK_DIRTY)) {
                block_unlockput(b);
                continue;
            }

            block_submit(b);

            if (!wait)
                block_put(b);
        }

        if (!wait)
            return;

        while (!list_empty(&sync_list)) {
            b = list_take_first(&sync_list, struct block, block_sync_node);

            /* Wait for block to be synced, and then drop reference */
            block_wait_for_sync(b);
            block_put(b);
        }
    }
}
