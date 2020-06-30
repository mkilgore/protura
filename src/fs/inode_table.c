/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/list.h>
#include <protura/hlist.h>
#include <protura/string.h>
#include <arch/spinlock.h>
#include <protura/mutex.h>
#include <protura/atomic.h>
#include <protura/mm/kmalloc.h>
#include <arch/task.h>

#include <protura/fs/block.h>
#include <protura/fs/super.h>
#include <protura/fs/file.h>
#include <protura/fs/stat.h>
#include <protura/fs/inode.h>
#include <protura/fs/vfs.h>

#ifdef CONFIG_KERNEL_LOG_INODE
# define kp_inode(str, ...) kp(KP_NORMAL, "Inode: " str, ## __VA_ARGS__)
#else
# define kp_inode(str, ...) do { } while (0)
#endif

#define INODE_HASH_SIZE 512

/* Protects inode_hashes, inode->hash_entry, inode_freeing_queue
 *
 * inode->flags_lock nests inside of this lock
 */
static spinlock_t inode_hashes_lock;
static hlist_head_t inode_hashes[INODE_HASH_SIZE];

/* This queue is used when an inode is in INO_FREEING.
 *
 * We cannot wait on the inode's queue because the inode will be free'd out
 * from under us. This single queue is for every inode, hopefully there's not
 * too many in INO_FREEING at any given time that are also be requested. */
static struct wait_queue inode_freeing_queue = WAIT_QUEUE_INIT(inode_freeing_queue);

static atomic_t inode_count;

/* Use the pointer and inode number for the hash. */
static inline int inode_hash_get(struct super_block *sb, ino_t ino)
{
    /* XOR the top and bottom of the pointer so that there's a bit more mixing
     * before we do the mod */
    uintptr_t sb_ptr = (uintptr_t)sb;
    sb_ptr = sb_ptr ^ ((sb_ptr >> 16) | (sb_ptr << 16));
    return (ino ^ sb_ptr) % INODE_HASH_SIZE;
}

static void __inode_hash_add(struct inode *new)
{
    int hash = inode_hash_get(new->sb, new->ino);
    hlist_add(inode_hashes + hash, &new->hash_entry);
    atomic_inc(&inode_count);
}

/* Allocates a completely empty inode */
struct inode *inode_create(struct super_block *sb)
{
    struct inode *new = sb->ops->inode_alloc(sb);
    new->sb = sb;

    atomic_inc(&new->ref);
    return new;
}

static void inode_deallocate(struct inode *i)
{
    i->sb->ops->inode_dealloc(i->sb, i);
}

static void __inode_kill(struct inode *i)
{
    hlist_del(&i->hash_entry);
    list_del(&i->sb_entry);
    list_del(&i->sb_dirty_entry);

    inode_deallocate(i);
}

static void __inode_wait_for_write(struct inode *inode)
{
    wait_queue_event_spinlock(&inode->flags_queue, !flag_test(&inode->flags, INO_SYNC), &inode->flags_lock);
}

void inode_wait_for_write(struct inode *inode)
{
    using_spinlock(&inode->flags_lock)
        __inode_wait_for_write(inode);
}

int inode_write_to_disk(struct inode *inode, int wait)
{
    using_spinlock(&inode->flags_lock) {
        if (!flag_test(&inode->flags, INO_DIRTY))
            return 0;

        if (flag_test(&inode->flags, INO_SYNC)) {
            /* Currently syncing, wait for sync to complete if passed `wait`,
             * otherwise just exit */
            if (wait)
                __inode_wait_for_write(inode);

            return 0;
        }

        flag_set(&inode->flags, INO_SYNC);
    }

    int ret;

    using_inode_lock_read(inode)
        ret = inode->sb->ops->inode_write(inode->sb, inode);

    using_spinlock(&inode_hashes_lock) {
        using_spinlock(&inode->flags_lock) {
            flag_clear(&inode->flags, INO_SYNC);
            flag_clear(&inode->flags, INO_DIRTY);
            list_del(&inode->sb_dirty_entry);
        }
    }

    wait_queue_wake(&inode->flags_queue);

    return ret;
}

static void inode_evict(struct inode *inode)
{
    if (inode->sb->ops->inode_delete) {
        int err = inode->sb->ops->inode_delete(inode->sb, inode);
        if (err)
            kp(KP_WARNING, "Error when deleting inode "PRinode"! Error: %d\n", Pinode(inode), err);
    }

    /* All done here, get rid of it */
    using_spinlock(&inode_hashes_lock)
        __inode_kill(inode);

    /* Notify anybody waiting for this inode to be free'd */
    wait_queue_wake(&inode_freeing_queue);
}

static void inode_finish(struct inode *inode)
{
    int err = inode_write_to_disk(inode, 1);
    if (err)
        kp(KP_WARNING, "Error when flushing inode "PRinode"! Error: %d\n", Pinode(inode), err);

    /* We're good here because nobody will take a reference while INO_FREEING
     * is set, so holding inode_hashes_lock means nobody else has access to the current inode */
    using_spinlock(&inode_hashes_lock)
        __inode_kill(inode);

    /* If someone was looking to use this inode for some reason, they'll be
     * waiting on inode_freeing_queue - wake them up so they can recreate the
     * inode */
    wait_queue_wake(&inode_freeing_queue);
}

void inode_put(struct inode *inode)
{
    spinlock_acquire(&inode_hashes_lock);
    spinlock_acquire(&inode->flags_lock);

    /* Check if the inode is completely gone, and remove it if so. If we're
     * already INO_FREEING then we don't do this - this could happen if a
     * INO_SYNC is currently happening */
    if (atomic_dec_and_test(&inode->ref) && !atomic_get(&inode->nlinks) && !flag_test(&inode->flags, INO_FREEING)) {
        kassert(!flag_test(&inode->flags, INO_SYNC), "INO_SYNC should not be set if inode->ref == 0!!!");

        flag_set(&inode->flags, INO_FREEING);

        spinlock_release(&inode->flags_lock);
        spinlock_release(&inode_hashes_lock);
        inode_evict(inode);
        return;
    }

    /* Add to proper dirty list if it's dirty */
    if (flag_test(&inode->flags, INO_DIRTY) && !list_node_is_in_list(&inode->sb_dirty_entry))
        list_add_tail(&inode->sb->dirty_inodes, &inode->sb_dirty_entry);

    spinlock_release(&inode->flags_lock);
    spinlock_release(&inode_hashes_lock);
}

struct inode *inode_dup(struct inode *i)
{
    atomic_inc(&i->ref);
    return i;
}

/* NOTE: Requires inode_hashes_lock and inode to be locked. Returns with only inode_hashes_lock locked */
static void wait_for_freeing(struct inode *inode)
{
    struct task *current = cpu_get_local()->current;

    scheduler_set_sleeping();
    wait_queue_register(&inode_freeing_queue, &current->wait);

    /* Note the release order is actually important, after inode_hashes_lock is
     * released `inode` may no longer be valid, and flags_lock cannot be held when
     * the inode is deallocated*/
    spinlock_release(&inode->flags_lock);
    spinlock_release(&inode_hashes_lock);

    scheduler_task_yield();

    /* Don't grab inode->flags_lock - we're not checking the flags here, and
     * the inode was presumably free'd while we were waiting */
    spinlock_acquire(&inode_hashes_lock);
    wait_queue_unregister(&current->wait);
    scheduler_set_running();
}

void inode_mark_valid(struct inode *new)
{
    using_spinlock(&new->flags_lock)
        flag_set(&new->flags, INO_VALID);

    wait_queue_wake(&new->flags_queue);
}

void inode_mark_bad(struct inode *new)
{
    using_spinlock(&inode_hashes_lock) {
        spinlock_acquire(&new->flags_lock);
        /* Don't bother signalling INO_BAD if there are no other references */
        if (atomic_dec_and_test(&new->ref)) {
            /* Drop lock before __inode_kill, since 'new' will be invalid
             * afterward */
            spinlock_release(&new->flags_lock);
            __inode_kill(new);
        } else {
            flag_set(&new->flags, INO_BAD);
            wait_queue_wake(&new->flags_queue);
            spinlock_release(&new->flags_lock);
        }
    }
}

/* Waits for an inode to become either valid or bad.
 *
 * If bad, it decrements the count, handles cleanup, and returns NULL.
 * If valid, return the inode
 */
static struct inode *inode_wait_for_valid_or_bad(struct inode *inode)
{
    using_spinlock(&inode->flags_lock)
        wait_queue_event_spinlock(&inode->flags_queue, inode->flags & F(INO_VALID, INO_BAD), &inode->flags_lock);

    /* We need inode_hashes_lock to be able to __inode_kill */
    spinlock_acquire(&inode_hashes_lock);
    spinlock_acquire(&inode->flags_lock);

    if (flag_test(&inode->flags, INO_BAD)) {
        int drop = atomic_dec_and_test(&inode->ref);

        spinlock_release(&inode->flags_lock);

        if (drop)
            __inode_kill(inode);

        inode = NULL;
    } else {
        spinlock_release(&inode->flags_lock);
    }

    spinlock_release(&inode_hashes_lock);

    return inode;
}

struct inode *inode_get_invalid(struct super_block *sb, ino_t ino)
{
    int hash = inode_hash_get(sb, ino);
    struct inode *inode, *new = NULL, *found = NULL;

  again:
    using_spinlock(&inode_hashes_lock) {
  again_no_acquire:
        hlist_foreach_entry(&inode_hashes[hash], inode, hash_entry) {
            if (inode->ino == ino && inode->sb == sb) {
                spinlock_acquire(&inode->flags_lock);

                if (flag_test(&inode->flags, INO_FREEING)) {
                    wait_for_freeing(inode);
                    goto again_no_acquire;
                }

                spinlock_release(&inode->flags_lock);
                atomic_inc(&inode->ref);

                found = inode;
                break;
            }
        }

        /* We have an allocated inode and did not find the one we were looking
         * for - add the new one to the hashtable and continue on to set it up */
        if (!found && new) {
            __inode_hash_add(new);
            list_add_tail(&sb->inodes, &new->sb_entry);
            found = new;
        }
    }

    /* Didn't find the inode, allocate a new one and try again */
    if (!found && !new) {
        new = inode_create(sb);
        new->ino = ino;
        goto again;
    }

    /* Found an inode, and it was not the one we allocated - deallocate if NULL */
    if (new && found != new)
        inode_deallocate(new);

    if (found && found == new)
        return found;

    /* Found an inode, did not create it - wait for INO_VALID or INO_BAD */
    if (found && found != new)
        found = inode_wait_for_valid_or_bad(found);

    /* The case of adding a new inode falls through to here - we return it with
     * the INO_VALID flag not set. */
    return found;
}

struct inode *inode_get(struct super_block *sb, ino_t ino)
{
    struct inode *inode = inode_get_invalid(sb, ino);
    if (!inode)
        return NULL;

    /* No locking necessary because INO_VALID is never unset after being set
     *
     * This case is hit if inode_get_invalid created a fresh inode */
    if (flag_test(&inode->flags, INO_VALID))
        return inode;

    /* If we added the new inode allocated by us, then fill it in and mark it valid */
    int ret = sb->ops->inode_read(sb, inode);
    if (ret) {
        kp(KP_WARNING, "ERROR reading inode "PRinode"!!!\n", Pinode(inode));
        inode_mark_bad(inode);
        return NULL;
    }

    inode_mark_valid(inode);
    return inode;
}

/* Protects the 'inode->sync_entry' members. Those are used to easily create
 * lists of inodes currently being written out or deleted, so that we can queue
 * them all up at once and then wait on them afterward. */
static mutex_t sync_lock = MUTEX_INIT(sync_lock);

void inode_sync(struct super_block *sb, int wait)
{
    list_head_t sync_list = LIST_HEAD_INIT(sync_list);
    struct inode *inode;

    using_mutex(&sync_lock) {
        using_spinlock(&inode_hashes_lock) {
            int hash;

            for (hash = 0; hash < INODE_HASH_SIZE; hash++) {
                hlist_foreach_entry(inode_hashes + hash, inode, hash_entry) {
                    if (sb && inode->sb != sb)
                        continue;

                    spinlock_acquire(&inode->flags_lock);

                    /* Check if we should actually be syncing this one before grabbing a reference */
                    if (flag_test(&inode->flags, INO_FREEING)
                            || !flag_test(&inode->flags, INO_VALID)
                            || !flag_test(&inode->flags, INO_DIRTY)) {

                        spinlock_release(&inode->flags_lock);
                        /* FIXME: 'wait' flag should trigger some waiting on INO_FREEING maybe? */
                        continue;
                    }
                    spinlock_release(&inode->flags_lock);

                    atomic_inc(&inode->ref);
                    list_add_tail(&sync_list, &inode->sync_entry);
                }
            }
        }

        while (!list_empty(&sync_list)) {
            inode = list_take_first(&sync_list, struct inode, sync_entry);

            inode_write_to_disk(inode, wait);
            inode_put(inode);
        }
    }
}

void inode_sync_all(int wait)
{
    return inode_sync(NULL, wait);
}

static void inode_finish_list(list_head_t *head)
{
    while (!list_empty(head)) {
        struct inode *i = list_take_first(head, struct inode, sync_entry);

        inode_finish(i);
    }
}

/* Removes all the inodes associated with a super_block that have no existing
 * references.
 *
 * The root inode gets some special handling - if we manage to remove every
 * inode from the super-block, then we check if we're holding the only root
 * inode reference and drop it if so */
int inode_clear_super(struct super_block *sb, struct inode *root)
{
    struct inode *inode;

  again:
    spinlock_acquire(&inode_hashes_lock);
  again_locked:
    list_foreach_entry(&sb->inodes, inode, sb_entry) {
        /* Skip any inodes with active references - should always include root */
        if (atomic_get(&inode->ref))
            continue;

        spinlock_acquire(&inode->flags_lock);

        /* If an inode is currently being free'd, wait for the signal and
         * start over, so that when inode_clear_super() returns the
         * inode is actually gone. */
        if (flag_test(&inode->flags, INO_FREEING)) {
            wait_for_freeing(inode);
            /* wait_for_freeing drops the inode->flags_lock for us */
            goto again_locked;
        }

        /* Can't happen - non-INO_VALID or INO_SYNC require at least one reference to exist */
        kassert(flag_test(&inode->flags, INO_VALID), "Inode "PRinode" has no references but is not marked INO_VALID!!!!!\n", Pinode(inode));
        kassert(!flag_test(&inode->flags, INO_SYNC), "Inode "PRinode" has no references but is marked INO_SYNC!!!!!\n", Pinode(inode));

        /* Nothing's using this inode, mark it and get rid of it */
        flag_set(&inode->flags, INO_FREEING);
        spinlock_release(&inode->flags_lock);
        spinlock_release(&inode_hashes_lock);

        inode_finish(inode);

        /* We dropped inode_hashes_lock and modified sb->inodes, so just start over. */
        goto again;
    }

    /* If there is more than one entry in here, the super was still in use */
    if (sb->inodes.next->next != &sb->inodes)
        goto release_hashes_lock;

    /* The case that root is the only reference still held, in which case ref>1 */
    if (atomic_get(&root->ref) != 1)
        goto release_hashes_lock;

    /* Verify sb->inodes only has one entry for the root inode left */
    if (list_empty(&sb->inodes)) {
        kp(KP_WARNING, "Super-Block has no inodes despite still having a reference to root!\n");
        goto release_hashes_lock;
    }

    if (sb->inodes.next != &root->sb_entry) {
        kp(KP_WARNING, "Super-Block's last inode is not a reference to root!\n");
        goto release_hashes_lock;
    }

    if (!atomic_get(&root->ref)) {
        kp(KP_WARNING, "Root's ref count is zero!\n");
        atomic_inc(&root->ref);
    }

    /* Drop root reference!
     *
     * We actually have a guarentee that the reference cannot be acquired again
     * even when we drop the hashes lock - the associated vfs_mount currently
     * has VFS_MOUNT_UNMOUNTING set. While that is set, the vfs_mount cannot be
     * used, and that is the only direct way to gain a reference to the
     * super-block's root.
     *
     * Thus, we drop it here, and then the caller will finish up the rest of
     * the umount.
     */

    using_spinlock(&root->flags_lock) {
        atomic_dec(&root->ref);
        flag_set(&root->flags, INO_FREEING);
    }

    spinlock_release(&inode_hashes_lock);

    inode_finish(root);

    return 0;


  release_hashes_lock:
    spinlock_release(&inode_hashes_lock);
    return -EBUSY;
}

void inode_oom(void)
{
    list_head_t finish_list = LIST_HEAD_INIT(finish_list);
    struct inode *inode;

    using_mutex(&sync_lock) {
        using_spinlock(&inode_hashes_lock) {
            int hash;

            for (hash = 0; hash < INODE_HASH_SIZE; hash++) {
                hlist_foreach_entry(inode_hashes + hash, inode, hash_entry) {
                    /* Skip any inodes with active references */
                    if (atomic_get(&inode->ref))
                        continue;

                    spinlock_acquire(&inode->flags_lock);
                    if (!flag_test(&inode->flags, INO_VALID)
                            || flag_test(&inode->flags, INO_FREEING)) {
                        spinlock_release(&inode->flags_lock);
                        continue;
                    }

                    kassert(flag_test(&inode->flags, INO_VALID), "Inode "PRinode" has no references but is not marked INO_VALID!!!!!\n", Pinode(inode));
                    kassert(!flag_test(&inode->flags, INO_SYNC), "Inode "PRinode" has no references but is marked INO_SYNC!!!!!\n", Pinode(inode));

                    flag_set(&inode->flags, INO_FREEING);
                    spinlock_release(&inode->flags_lock);

                    list_add_tail(&finish_list, &inode->sync_entry);
                }
            }
        }

        inode_finish_list(&finish_list);
    }
}

#ifdef CONFIG_KERNEL_TESTS
# include "inode_table_test.c"
#endif
