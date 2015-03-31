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
#include <fs/stat.h>

typedef uint32_t ino_t;
typedef uint32_t dev_t;
typedef uint16_t umode_t;

struct inode_ops;

struct inode {
    ino_t i_ino;
    dev_t i_dev;
    off_t i_size;
    umode_t i_mode;

    struct inode_ops *ops;

    atomic32_t ref;

    uint32_t links;

     /* Entry in either the inode hash table, or free inode table */
    struct list_node free_entry;
    struct hlist_node hash_entry;
};

struct inode_ops {
    struct file_ops *default_file_ops;
    int (*create) (struct inode *dir, const char *, int len, umode_t mode, struct inode **result);
};

struct inode *inode_new(void);
void inode_free(struct inode *);

#endif
