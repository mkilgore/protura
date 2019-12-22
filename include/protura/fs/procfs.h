/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_PROCFS_H
#define INCLUDE_FS_PROCFS_H

#include <protura/types.h>
#include <protura/time.h>
#include <protura/mutex.h>
#include <protura/list.h>
#include <protura/hlist.h>
#include <protura/fs/inode.h>
#include <protura/fs/stat.h>
#include <protura/fs/file.h>

/* 
 * procfs_dir and procfs_entry locks can be taken at the same time, as long as
 * they are taken in the same order as the file-system. IE. If a 'dir2' is a
 * decendent of 'dir1', then you can take both of their locks as long as you
 * lock 'dir1' first.
 */

struct procfs_node {
    char *name;
    size_t len;

    ino_t ino;
    mode_t mode;
    int nlinks;
    time_t ctime;
    struct procfs_dir *parent;

    mutex_t lock;

    list_node_t parent_node;
    hlist_node_t inode_hash_entry;
};

struct procfs_entry_ops {
    int (*readpage) (void *page, size_t page_size, size_t *len);

    int (*read) (struct file *filp, struct user_buffer buf, size_t);
    int (*ioctl) (struct file *filp, int cmd, struct user_buffer ptr);
};

struct procfs_entry {
    struct procfs_node node;
    const struct procfs_entry_ops *ops;
    const struct file_ops *file_ops;
};

struct procfs_dir {
    struct procfs_node node;
    int entry_count;
    list_head_t entry_list;
};

#define PROCFS_NODE_INIT(node)\
    { \
        .inode_hash_entry = HLIST_NODE_INIT(), \
        .lock = MUTEX_INIT((node).lock), \
        .parent_node = LIST_NODE_INIT((node).parent_node), \
    }

#define PROCFS_ENTRY_INIT(entry) \
    { \
        .node = PROCFS_NODE_INIT((entry).node), \
    }

static inline void procfs_entry_init(struct procfs_entry *entry)
{
    *entry = (struct procfs_entry)PROCFS_ENTRY_INIT(*entry);
}

#define PROCFS_DIR_INIT(dir) \
    { \
        .node = PROCFS_NODE_INIT((dir).node), \
        .entry_list = LIST_HEAD_INIT((dir).entry_list), \
    }

static inline void procfs_dir_init(struct procfs_dir *dir)
{
    *dir = (struct procfs_dir)PROCFS_DIR_INIT(*dir);
}

struct procfs_inode {
    struct inode i;
    struct procfs_node *node;
};

void procfs_register_entry_ops(struct procfs_dir *parent, const char *name, const struct procfs_entry_ops *ops);
void procfs_register_entry(struct procfs_dir *parent, const char *name, const struct file_ops *ops);
struct procfs_dir *procfs_register_dir(struct procfs_dir *parent, const char *name);

struct procfs_dir procfs_root;

void procfs_init(void);

#endif
