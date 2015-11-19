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
#include <mm/kmalloc.h>

#include <arch/spinlock.h>
#include <fs/block.h>
#include <fs/char.h>
#include <fs/stat.h>
#include <fs/file.h>
#include <fs/inode_table.h>
#include <fs/file_system.h>
#include <fs/ext2.h>
#include "ext2_internal.h"

struct file_ops ext2_file_ops_file = {
    .open = NULL,
    .release = NULL,
    .read = fs_file_generic_read,
    .write = NULL,
    .lseek = fs_file_generic_lseek,
    .readdir = NULL,
};

struct inode_ops ext2_inode_ops_file = {
    .lookup = NULL,
    .change_attrs = NULL,
    .bmap = ext2_bmap,
};

