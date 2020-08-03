#ifndef INCLUDE_PROTURA_BLOCK_BCACHE_H
#define INCLUDE_PROTURA_BLOCK_BCACHE_H

#include <protura/types.h>
#include <protura/stddef.h>
#include <protura/wait.h>
#include <protura/list.h>
#include <protura/hlist.h>
#include <protura/scheduler.h>
#include <protura/mutex.h>
#include <protura/atomic.h>
#include <protura/dev.h>

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

    /* Location of this block on the block device */
    sector_t sector;

    /* Location of this block with any partition information taken into account */
    sector_t real_sector;

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

struct block *block_get(struct block_device *bdev, sector_t);
void block_put(struct block *);
void block_wait_for_sync(struct block *);

static inline struct block *block_dup(struct block *b)
{
    atomic_inc(&b->refs);
    return b;
}

static inline struct block *block_getlock(struct block_device *bdev, sector_t sec)
{
    struct block *b = block_get(bdev, sec);
    block_lock(b);
    return b;
}

static inline void block_unlockput(struct block *b)
{
    block_unlock(b);
    block_put(b);
}

#define using_block(bdev, sector, block) \
    using_nocheck(((block) = block_get(bdev, sector)), (block_put(block)))

#define using_block_locked(bdev, sector, block) \
    using_nocheck(((block) = block_getlock(bdev, sector)), (block_unlockput(block)))

void bdflush_init(void);

#endif
