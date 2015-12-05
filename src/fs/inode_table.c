/*
 * Copyright (C) 2015 Matt Kilgore
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
#include <protura/fs/inode_table.h>
#include <protura/fs/vfs.h>

#define INODE_HASH_SIZE 512

static struct {
    spinlock_t lock;

    struct hlist_head inode_hashes[INODE_HASH_SIZE];

    /* Entries in this list have already been flushed to the disk and have no
     * references. */
    list_head_t unused;

    atomic_t inode_count;
} inode_list = {
    .lock = SPINLOCK_INIT("Inode table lock"),
    .inode_hashes = { { .first = NULL } },
    .unused = LIST_HEAD_INIT(inode_list.unused),
    .inode_count = ATOMIC_INIT(0),
};

static inline int inode_hash_get(dev_t dev, ino_t ino)
{
    return (ino ^ (DEV_MAJOR(dev) + DEV_MINOR(dev))) % INODE_HASH_SIZE;
}

static void inode_hash(struct inode *inode)
{
    int hash = inode_hash_get(inode->dev, inode->ino);
    if (hlist_hashed(&inode->hash_entry))
        hlist_del(&inode->hash_entry);

    using_spinlock(&inode_list.lock)
        hlist_add(inode_list.inode_hashes + hash, &inode->hash_entry);
}

void inode_oom(void)
{
    struct inode *inode;

    /* Entries are already flushed to the disk beforehand. This just removes
     * them from the hash and frees their memory. */
    using_spinlock(&inode_list.lock) {
        list_foreach_take_entry(&inode_list.unused, inode, list_entry) {
            struct super_block *sb = inode->sb;
            hlist_del(&inode->hash_entry);
            sb->ops->inode_dealloc(sb, inode);
        }
    }
}

void inode_put(struct inode *inode)
{
    int release = 0;
    struct super_block *sb = inode->sb;

    using_spinlock(&inode_list.lock) {
        if (atomic_get(&inode->ref) == 1) {
            /* We hold the only reference to this inode, so a try_lock should
             * always work. We can't risk a regular lock though, because we
             * can't sleep with inode_list.lock.
             *
             * Note that doing inode_put while holding the lock is illegal. */
            if (inode_try_lock_write(inode))
                release = 1;
            else
                panic("Inode lock error! inode_put with ref=1 while holding the lock.\n");
        } else {
            atomic_dec(&inode->ref);
        }
    }

    if (release) {
        /* If this inode has no hard-links to it, then we can just discard it. */
        if (!inode->nlinks) {
            sb->ops->inode_delete(sb, inode);
            sb->ops->inode_dealloc(sb, inode);
            return ;
        }

        sb->ops->inode_write(sb, inode);

        using_spinlock(&inode_list.lock) {

            /* Check the reference count again. Since inode_write can sleep
             * it's possible there are more references. Note that checking the
             * inode for valid and dirty is not necessary.
             */
            if (atomic_dec_and_test(&inode->ref))
                list_add_tail(&inode_list.unused, &inode->list_entry);

            inode_unlock_write(inode);
        }
    }

    return ;
}

struct inode *inode_dup(struct inode *i)
{
    using_spinlock(&inode_list.lock)
        atomic_inc(&i->ref);

    return i;
}

struct inode *inode_get(struct super_block *sb, ino_t ino)
{
    int hash = inode_hash_get(sb->dev, ino);
    struct inode *inode = NULL;

    using_spinlock(&inode_list.lock) {
        hlist_foreach_entry(&inode_list.inode_hashes[hash], inode, hash_entry) {
            if (inode->ino == ino && inode->dev == sb->dev) {
                atomic_inc(&inode->ref);
                list_del(&inode->list_entry);

                break;
            }
        }
    }

    if (inode)
        goto return_inode;

    inode = sb_inode_read(sb, ino);

    kp(KP_TRACE, "Read inode: %p\n", inode);

    if (!inode)
        return NULL;

    atomic_inc(&inode->ref);
    atomic_inc(&inode_list.inode_count);

    inode_hash(inode);

  return_inode:
    return inode;
}

