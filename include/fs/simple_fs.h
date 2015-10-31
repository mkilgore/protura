/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_SIMPLE_FS_H
#define INCLUDE_FS_SIMPLE_FS_H

#include <protura/types.h>

#ifdef __KERNEL__

#include <fs/inode.h>
#include <fs/super.h>

struct simple_fs_inode {
    struct inode i;

    sector_t contents[100];
};

struct simple_fs_super_block {
    struct super_block sb;

    uint32_t inode_count;
    ino_t root_ino;
    sector_t inode_map;
};

struct super_block *simple_fs_read_sb(dev_t);
void simple_fs_init(void);

extern struct inode_ops simple_fs_inode_ops;
extern struct file_ops simple_fs_file_ops;

#endif

/* Disk format:
 *
 * sb: ->
 *   inode_count
 *   root_ino
 *   inode_map_sector
 * 
 * inode_map_sector ->
 *   inode:sector -> simple_fs_disk_inode
 *   inode:sector -> simple_fs_disk_inode
 *   ...
 *
 * simple_fs_disk_inode ->
 *   size
 *   sectors[0] -> data
 *   sectors[1] -> data
 *   ...
 */

/* Super-block format on disk */
struct simple_fs_disk_sb {
    uint32_t inode_count;
    uint32_t root_ino;
    uint32_t inode_map_sector;
};

/* Exactly 512 bytes big. */
struct simple_fs_disk_inode {
    uint32_t size;
    mode_t mode;
    unsigned short major;
    unsigned short minor;
    uint32_t sectors[100];
};

struct simple_fs_disk_inode_map {
    uint32_t ino;
    uint32_t sector;
};

#endif
