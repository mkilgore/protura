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

#include <fs/inode.h>

#define INODE_HASH_SIZE 512

static struct {
    struct spinlock lock;
    struct list_head free_inodes;

    struct hlist_head inode_hashes[INODE_HASH_SIZE];

    ino_t next_ino;
} inode_list = {
    .lock = SPINLOCK_INIT("Inode table lock"),
    .free_inodes = LIST_HEAD_INIT(inode_list.free_inodes),
    .inode_hashes = { { .first = NULL } },
    .next_ino = 1,
};

#define INODE_LIST_GROW_COUNT 20

static inline int inode_hash_get(dev_t dev, ino_t ino)
{
    return (ino ^ dev) % INODE_HASH_SIZE;
}

static void inode_hash(struct inode *inode)
{
    int hash = inode_hash_get(inode->i_dev, inode->i_ino);
    if (hlist_hashed(&inode->hash_entry))
        hlist_del(&inode->hash_entry);

    using_spinlock(&inode_list.lock)
        hlist_add(inode_list.inode_hashes + hash, &inode->hash_entry);
}

static void inode_list_grow(void)
{
    int i;

    using_spinlock(&inode_list.lock) {
        for (i = 0; i < INODE_LIST_GROW_COUNT; i++) {
            struct inode *inode = kmalloc(sizeof(*inode), PMAL_KERNEL);
            memset(inode, 0, sizeof(*inode));
            list_add(&inode_list.free_inodes, &inode->free_entry);
        }
    }
}

struct inode *inode_new(void)
{
    struct inode *inode;
    int empty;

    using_spinlock(&inode_list.lock)
        empty = list_empty(&inode_list.free_inodes);

    if (empty)
        inode_list_grow();

    using_spinlock(&inode_list.lock) {
        inode = list_take_first(&inode_list.free_inodes, struct inode, free_entry);
        inode->i_ino = inode_list.next_ino++;
    }

    return inode;
}

void inode_put(struct inode *inode)
{
    atomic32_dec(&inode->ref);
}

struct inode *inode_get(dev_t dev, ino_t ino)
{
    int hash = inode_hash_get(dev, ino);
    struct inode *inode;
    using_spinlock(&inode_list.lock)
        hlist_foreach_entry(&inode_list.inode_hashes[hash], inode, hash_entry)
            if (inode->i_ino == ino && inode->i_dev == dev)
                break;

    if (inode)
        atomic32_inc(&inode->ref);
    return inode;
}

void inode_free(struct inode *inode)
{

    memset(inode, 0, sizeof(*inode));
    using_spinlock(&inode_list.lock)
        list_add(&inode_list.free_inodes, &inode->free_entry);
}

