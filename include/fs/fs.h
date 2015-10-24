/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_FS_H
#define INCLUDE_FS_FS_H

#include <protura/types.h>
#include <protura/errors.h>

#include <fs/block.h>
#include <fs/file_system.h>
#include <fs/super.h>
#include <fs/inode.h>
#include <fs/inode_table.h>
#include <fs/file.h>
#include <fs/dirent.h>
#include <fs/stat.h>
#include <fs/vfs.h>

extern struct inode *ino_root;

static inline int root_lookup(const char *path, size_t len, struct inode **result)
{
    return vfs_lookup(ino_root, path, len, result);
}

#endif
