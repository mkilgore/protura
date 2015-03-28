/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_INODE_H
#define INCLUDE_FS_INODE_H

struct inode;

#include <protura/types.h>
#include <protura/list.h>
#include <protura/hlist.h>
#include <protura/atomic.h>
#include <fs/vfsnode.h>

typedef uint32_t ino_t;
typedef uint32_t dev_t;

enum inode_type {
    FS_FILE,
    FS_DIR,
    FS_DEV,
};

struct inode_ops;

struct inode {
    ino_t i_ino;
    dev_t i_dev;
    off_t i_size;
    struct inode_ops *ops;
    enum inode_type type;

    atomic32_t ref;

    uint32_t links;
    struct list_head vfsnodes; /* List of vfsnode's assiciated with this inode */

     /* Entry in either the inode hash table, or free inode table */
    union {
        struct list_node inode_list;
        struct hlist_node hash_entry;
    };

    void *inode_data;
};

struct inode_ops {
    int (*truncate) (struct inode *, size_t len);
    int (*release) (struct inode *);
};


struct inode *inode_new(void);
void inode_free(struct inode *);

#endif
