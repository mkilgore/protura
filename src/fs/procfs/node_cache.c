/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

/*
 * node cache
 *
 * This is a hash-table of all the struct procfs_node entries, hashed by
 * inode-number.
 *
 * The use of this is debatable. The struct procfs_node related to an inode is
 * stored in the extended version of the inode itself, meaning the only time
 * this really gets used is in procfs_read_inode, where it is used to find the
 * associated node to the inode being created. That shouldn't be a common
 * operation, and procfs isn't really that big, so a linear look-up might be
 * just as good with less work.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/list.h>
#include <protura/mm/kmalloc.h>

#include <arch/spinlock.h>
#include <protura/block/bcache.h>
#include <protura/fs/char.h>
#include <protura/fs/stat.h>
#include <protura/fs/file.h>
#include <protura/fs/file_system.h>
#include <protura/fs/vfs.h>
#include <protura/fs/procfs.h>
#include "procfs_internal.h"

static ino_t next_ino = 2;

ino_t procfs_next_ino(void)
{
    return next_ino++;
}

/* Extremely simple hash - just the inode number % table_size. This is probably
 * good enough anyway though, because inode numbers are sequential, so the
 * table will be as evenly distributed as you could get */
#define PROCFS_HASH_TABLE_SIZE 128

static spinlock_t procfs_hashtable_lock = SPINLOCK_INIT();
static hlist_head_t procfs_hashtable[PROCFS_HASH_TABLE_SIZE];

/* Note: Already hold entry_lock on inode */
static int procfs_hash_node(ino_t ino)
{
    return ino % PROCFS_HASH_TABLE_SIZE;
}

void procfs_hash_add_node(struct procfs_node *node)
{
    using_spinlock(&procfs_hashtable_lock)
        hlist_add(&procfs_hashtable[procfs_hash_node(node->ino)], &node->inode_hash_entry);
}

struct procfs_node *procfs_hash_get_node(ino_t ino)
{
    struct procfs_node *node = NULL;

    using_spinlock(&procfs_hashtable_lock) {
        hlist_foreach_entry(&procfs_hashtable[procfs_hash_node(ino)], node, inode_hash_entry) {
            if (node->ino == ino)
                break;
        }
    }

    return node;
}

