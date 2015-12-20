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
    mutex_t lock;

    struct hlist_head inode_hashes[INODE_HASH_SIZE];

    /* Entries in this list have already been flushed to the disk and have no
     * references. */
    list_head_t unused;

    atomic_t inode_count;
} inode_list = {
    .lock = MUTEX_INIT(inode_list.lock, "inode-hash-list"),
    .inode_hashes = { { .first = NULL } },
    .unused = LIST_HEAD_INIT(inode_list.unused),
    .inode_count = ATOMIC_INIT(0),
};

static inline int inode_hash_get(dev_t dev, ino_t ino)
{
    return (ino ^ (DEV_MAJOR(dev) + DEV_MINOR(dev))) % INODE_HASH_SIZE;
}

void inode_oom(void)
{
    struct inode *inode;

    /* Entries are already flushed to the disk beforehand. This just removes
     * them from the hash and frees their memory. */
    using_mutex(&inode_list.lock) {
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

    using_mutex(&inode_list.lock) {
        if (atomic_get(&inode->ref) == 1) {
            /* We hold the only reference to this inode, so a try_lock should
             * always work. If it doesn't then someone used inode_put() on the
             * only reference left while holding a read/write lock on it, which
             * is illegal */
            if (inode_try_lock_write(inode))
                release = 1;
            else
                panic("Inode lock error! inode_put with ref=1 while holding the lock.\n");
        } else {
            atomic_dec(&inode->ref);
        }
    }

    if (release) {
        kp(KP_TRACE, "Releasing inode "PRinode"\n", Pinode(inode));
        /* If this inode has no hard-links to it, then we can just discard it. */
        if (atomic32_get(&inode->nlinks) == 0) {
            sb->ops->inode_delete(sb, inode);
            sb->ops->inode_dealloc(sb, inode);
            return ;
        }

        using_mutex(&inode->sb->dirty_inodes_lock) {
            if (flag_test(&inode->flags, INO_DIRTY)) {
                sb->ops->inode_write(sb, inode);
                list_del(&inode->sb_dirty_entry);
            }
        }

        using_mutex(&inode_list.lock) {

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
    using_mutex(&inode_list.lock)
        atomic_inc(&i->ref);

    return i;
}

struct inode *inode_get(struct super_block *sb, ino_t ino)
{
    int hash = inode_hash_get(sb->dev, ino);
    struct inode *inode = NULL;

    using_mutex(&inode_list.lock) {
        hlist_foreach_entry(&inode_list.inode_hashes[hash], inode, hash_entry) {
            if (inode->ino == ino && inode->sb_dev == sb->dev) {
                list_del(&inode->list_entry);
                atomic_inc(&inode->ref);

                break;
            }
        }

        if (!inode) {
            int ret;

            /* FIXME: It's necessary to call sb_inode_read while holding the
             * inode_list.lock to avoid a race, however the race could be
             * avoided without doing that as long as a way of handle multipel
             * inode_get() calls for the same inode number is handled */
            inode = sb->ops->inode_alloc(sb);
            inode->ino = ino;

            ret = sb->ops->inode_read(sb, inode);
            kp(KP_TRACE, "Read inode: %p - "PRinode"\n", inode, Pinode(inode));

            if (ret) {
                sb->ops->inode_delete(sb, inode);
                inode = NULL;
            } else {
                hlist_add(inode_list.inode_hashes + hash, &inode->hash_entry);
                atomic_inc(&inode_list.inode_count);
                atomic_inc(&inode->ref);
            }
        }
    }

    return inode;
}

