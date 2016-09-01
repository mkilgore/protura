/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/list.h>
#include <protura/mutex.h>
#include <protura/mm/kmalloc.h>

#include <arch/spinlock.h>
#include <protura/fs/block.h>
#include <protura/fs/char.h>
#include <protura/fs/stat.h>
#include <protura/fs/file.h>
#include <protura/fs/inode_table.h>
#include <protura/fs/file_system.h>
#include <protura/fs/vfs.h>
#include <protura/fs/procfs.h>
#include "procfs_internal.h"

static void procfs_add_node(struct procfs_dir *parent, struct procfs_node *node)
{
    using_mutex(&parent->node.lock) {
        using_mutex(&node->lock) {
            parent->node.nlinks++;
            node->nlinks = 1;

            procfs_hash_add_node(node);

            list_add(&parent->entry_list, &node->parent_node);
            parent->entry_count++;
        }
    }
}

void procfs_register_entry(struct procfs_dir *parent, const char *name, int (*readpage) (void *page, size_t page_size, size_t *len))
{
    struct procfs_entry *entry;

    entry = kmalloc(sizeof(*entry), PAL_KERNEL);

    kp(KP_TRACE, "entry: %p\n", entry);
    kp(KP_TRACE, "parent: %p\n", parent);

    procfs_entry_init(entry);

    entry->node.name = name;
    entry->node.len = strlen(name);
    entry->node.mode = S_IFREG | 0777;
    entry->node.parent = parent;
    entry->node.ctime = protura_current_time_get();
    entry->node.ino = procfs_next_ino();
    entry->readpage = readpage;

    procfs_add_node(parent, &entry->node);
}

void procfs_register_entry_read(struct procfs_dir *parent, const char *name, int (*read) (struct file *filp, void *, size_t))
{
    struct procfs_entry *entry;

    entry = kmalloc(sizeof(*entry), PAL_KERNEL);

    procfs_entry_init(entry);

    entry->node.name = name;
    entry->node.len = strlen(name);
    entry->node.mode = S_IFREG | 0777;
    entry->node.parent = parent;
    entry->node.ctime = protura_current_time_get();
    entry->node.ino = procfs_next_ino();
    entry->read = read;

    procfs_add_node(parent, &entry->node);
}

struct procfs_dir *procfs_register_dir(struct procfs_dir *parent, const char *name)
{
    struct procfs_dir *new;

    new = kmalloc(sizeof(*new), PAL_KERNEL);

    kp(KP_TRACE, "new: %p\n", new);
    kp(KP_TRACE, "parent: %p\n", parent);

    procfs_dir_init(new);
    new->node.name = name;
    new->node.len = strlen(name);
    new->node.mode = S_IFDIR | 0777;
    new->node.parent = parent;
    new->node.ctime = protura_current_time_get();
    new->node.ino = procfs_next_ino();

    procfs_add_node(parent, &new->node);

    return new;
}

struct procfs_dir procfs_root = {
    .node = {
        .name = "",
        .len = 0,
        .ino = PROCFS_ROOT_INO,
        .mode = S_IFDIR | 0777,
        .nlinks = 1,
        .parent = &procfs_root,
        .parent_node = LIST_NODE_INIT(procfs_root.node.parent_node),
        .lock = MUTEX_INIT(procfs_root.node.lock, "procfs-root-node-lock"),
        .inode_hash_entry = HLIST_NODE_INIT(),
    },
    .entry_list = LIST_HEAD_INIT(procfs_root.entry_list),
};

