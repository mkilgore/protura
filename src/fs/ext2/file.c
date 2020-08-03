/*
 * Copyright (C) 2015 Matt Kilgore
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
#include <protura/dump_mem.h>
#include <protura/mm/kmalloc.h>

#include <arch/spinlock.h>
#include <protura/block/bcache.h>
#include <protura/fs/char.h>
#include <protura/fs/stat.h>
#include <protura/fs/file.h>
#include <protura/fs/file_system.h>
#include <protura/fs/ext2.h>
#include "ext2_internal.h"

struct file_ops ext2_file_ops_file = {
    .open = NULL,
    .release = NULL,
    .pread = fs_file_generic_pread,
    .read = fs_file_generic_read,
    .write = fs_file_generic_write,
    .lseek = fs_file_generic_lseek,
    .readdir = NULL,
    .ioctl = fs_file_ioctl,
};

struct inode_ops ext2_inode_ops_file = {
    .lookup = NULL,
    .truncate = ext2_truncate,
    .bmap = ext2_bmap,
    .bmap_alloc = ext2_bmap_alloc,
};

