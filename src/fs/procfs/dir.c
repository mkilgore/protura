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
#include <protura/fs/inode.h>
#include <protura/fs/inode_table.h>
#include <protura/fs/file_system.h>
#include <protura/fs/vfs.h>
#include <protura/fs/procfs.h>
#include "procfs_internal.h"

static int procfs_inode_dir_lookup(struct inode *dir, const char *name, size_t len, struct inode **result)
{
    struct procfs_inode *pdir = container_of(dir, struct procfs_inode, i);
    struct procfs_node *node;
    struct procfs_dir *dir_node;
    struct procfs_node *next, *found = NULL;

    if (strncmp(name, ".", len) == 0) {
        *result = inode_dup(dir);
        return 0;
    }

    node = pdir->node;
    dir_node = container_of(node, struct procfs_dir, node);

    if (strncmp(name, "..", len) == 0) {
        *result = inode_get(dir->sb, dir_node->node.parent->node.ino);
        return 0;
    }

    using_mutex(&dir_node->node.lock) {
        list_foreach_entry(&dir_node->entry_list, next, parent_node) {
            if (next->len == len && strncmp(name, next->name, len) == 0) {
                found = next;
                break;
            }
        }
    }

    if (!found)
        return -ENOENT;

    *result = inode_get(dir->sb, found->ino);

    return 0;
}

struct inode_ops procfs_dir_inode_ops = {
    .lookup = procfs_inode_dir_lookup,
};

static int fill_dent(struct dent *dent, size_t dent_size, ino_t ino, const char *name, size_t name_len)
{
    size_t required_size = sizeof(struct dent) + name_len + 1;

    if (required_size > dent_size)
        return -EINVAL;

    dent->ino = ino;
    dent->dent_len = required_size;
    dent->name_len = name_len;
    memcpy(dent->name, name, name_len);
    dent->name[name_len] = '\0';

    return 0;
}

static int procfs_inode_dir_read_dent(struct file *filp, struct dent *dent, size_t dent_size)
{
    int ret = 0;
    int count = filp->offset - 2;
    struct procfs_inode *pinode = container_of(filp->inode, struct procfs_inode, i);
    struct procfs_node *node, *next, *found = NULL;
    struct procfs_dir *dir_node;

    switch (filp->offset) {
    case 0:
        ret = fill_dent(dent, dent_size, filp->inode->ino, ".", 1);
        break;

    case 1:
        ret = fill_dent(dent, dent_size, filp->inode->ino, "..", 2);
        break;

    default:
        node = pinode->node;
        dir_node = container_of(node, struct procfs_dir, node);

        using_mutex(&dir_node->node.lock) {
            list_foreach_entry(&dir_node->entry_list, next, parent_node) {
                if (count == 0) {
                    found = next;
                    break;
                } else {
                    count--;
                }
            }
        }

        if (found)
            ret = fill_dent(dent, dent_size, found->ino, found->name, found->len);
        else
            ret = -EINVAL;
        break;
    }

    if (!ret)
        filp->offset++;

    pinode->i.atime = protura_current_time_get();

    if (ret)
        return ret;
    else
        return dent->dent_len;
}

struct file_ops procfs_dir_file_ops = {
    .read_dent = procfs_inode_dir_read_dent,
    .lseek = fs_file_generic_lseek,
};

