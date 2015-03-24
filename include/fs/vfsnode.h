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
#include <fs/inode.h>

struct vfsnode_ops;

struct vfsnode {
    struct inode *inode; /* The inode containing the data for this vfsnode */
    char name[256];

    struct vfsnode *parent;
    struct list_head children;
    struct vfsnode_ops *ops;
};

struct vfsnode_ops {

};

struct vfsnode *vfsnode_get_new(void);
void dentry_free(struct vfsnode *);

#endif
