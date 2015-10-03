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
#include <protura/spinlock.h>
#include <protura/atomic.h>
#include <mm/kmalloc.h>

#include <fs/block.h>
#include <fs/super.h>
#include <fs/inode.h>

#define INODE_HASH_SIZE 512

static struct {
    struct spinlock lock;

    struct hlist_head inode_hashes[INODE_HASH_SIZE];

    kino_t next_ino;
} inode_list = {
    .lock = SPINLOCK_INIT("Inode table lock"),
    .inode_hashes = { { .first = NULL } },
    .next_ino = 1,
};

#define INODE_LIST_GROW_COUNT 20

static inline int inode_hash_get(kdev_t dev, kino_t ino)
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

void inode_put(struct inode *inode)
{
    struct super_block *sb = inode->sb;
    atomic32_dec(&inode->ref);

    using_inode(inode)
        if (inode->dirty && atomic32_get(&inode->ref) == 0)
            sb->ops->write_inode(sb, inode);
}

struct inode *inode_get(struct super_block *sb, kino_t ino)
{
    int hash = inode_hash_get(sb->dev, ino);
    struct inode *inode;

    using_spinlock(&inode_list.lock)
        hlist_foreach_entry(&inode_list.inode_hashes[hash], inode, hash_entry)
            if (inode->ino == ino && inode->dev == sb->dev)
                break;

    if (inode)
        goto return_inode;

    inode = sb->ops->read_inode(sb, ino);

    inode_hash(inode);

  return_inode:
    atomic32_inc(&inode->ref);
    return inode;
}

