/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_SUPER_H
#define INCLUDE_FS_SUPER_H

#ifdef __KERNEL__

#include <fs/block.h>
#include <fs/dirent.h>

struct inode;
struct super_block;

struct super_block_ops {
    struct inode *(*read_inode) (struct super_block *, kino_t);
    void (*write_inode) (struct super_block *, struct inode *);
    void (*put_inode) (struct super_block *, struct inode *);
    void (*delete_inode) (struct super_block *, struct inode *);

    void (*write_sb) (struct super_block *);
    void (*put_sb) (struct super_block *);
};

struct super_block {
    kdev_t dev;
    struct block_device *bdev;
    struct inode *root;

    struct super_block_ops *ops;
};

#endif

#endif
