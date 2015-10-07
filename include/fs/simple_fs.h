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

    ksector_t contents[12];
};

struct simple_fs_super_block {
    struct super_block sb;

    uint32_t file_count;
    ksector_t root_ino;
};

struct super_block *simple_fs_read_sb(kdev_t);
void simple_fs_init(void);

#endif

/* Super-block format on disk */
struct simple_fs_disk_sb {
    uint32_t file_count;
    uint32_t root_ino;
    uint32_t inode_count;
    uint32_t inode_map_sector;
};

/* Exactly 512 bytes big. */
struct simple_fs_disk_inode {
    uint32_t size;
    uint32_t sectors[12];
};

struct simple_fs_disk_inode_map {
    uint32_t ino;
    uint32_t sector;
};

#endif
