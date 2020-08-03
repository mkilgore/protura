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

#include <protura/fs/file_system.h>
#include <protura/fs/super.h>
#include <protura/fs/inode.h>
#include <protura/fs/file.h>
#include <protura/fs/dirent.h>
#include <protura/fs/stat.h>
#include <protura/fs/super.h>
#include <protura/fs/vfs.h>

extern struct super_block *sb_root;
extern struct inode *ino_root;

static inline int root_lookup(const char *path, size_t len, struct inode **result)
{
    return vfs_lookup(ino_root, path, len, result);
}

#endif
