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
#include <fs/vfsnode.h>

typedef uint32_t ino_t;

enum inode_type {
    FS_FILE,
    FS_DIR,
};

struct inode_ops;

struct inode {
    ino_t ino;
    enum inode_type type;

    struct list_head vfsnodes; /* List of dentry's assiciated with this inode */

    struct list_node inode_list;

    unsigned int mask;
    unsigned int i_uid;
    unsigned int i_gid;

    off_t i_size;

    struct inode_ops *ops;

    void *inode_data;
};

struct inode_ops {
    int (*truncate) (struct inode *, size_t len);
    int (*release) (struct inode *);
};


struct inode *inode_new(void);
void inode_free(struct inode *);

#endif
