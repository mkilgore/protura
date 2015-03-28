/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_VFSNODE_H
#define INCLUDE_FS_VFSNODE_H

struct vfsnode;

#include <protura/types.h>
#include <protura/list.h>
#include <protura/spinlock.h>
#include <fs/inode.h>

struct vfsnode_ops;

struct vfsnode {
    struct inode *inode; /* The inode containing the data for this vfsnode */
    struct spinlock lock;
    char name[256];

    struct vfsnode *parent;
    struct list_head children;
    size_t child_count;

    struct list_node child_entry;
    struct vfsnode_ops *ops;

    struct list_node empty;
};

struct vfsnode_ops {

};

extern struct vfsnode *vfs_root;

struct vfsnode *vfsnode_get_new(void);
void vfsnode_free(struct vfsnode *);

struct vfsnode *vfsnode_find(const char *path);

struct vfsnode *vfs_find(const char *path);
void vfs_init(void);

#endif
