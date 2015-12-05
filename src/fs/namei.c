/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/errors.h>
#include <protura/debug.h>
#include <protura/list.h>
#include <protura/hlist.h>
#include <protura/string.h>
#include <arch/spinlock.h>
#include <protura/atomic.h>
#include <protura/mm/kmalloc.h>

#include <protura/fs/block.h>
#include <protura/fs/super.h>
#include <protura/fs/file.h>
#include <protura/fs/inode.h>
#include <protura/fs/inode_table.h>
#include <protura/fs/namei.h>
#include <protura/fs/fs.h>

int namex(const char *path, struct inode *cwd, struct inode **result)
{
    int ret = 0;

    if (*path == '/') {
        cwd = ino_root;
        path++;
    }

    cwd = inode_dup(cwd);

    while (*path) {
        struct inode *next;
        size_t len = 0;

        while (path[len] && path[len] != '/')
            len++;

        ret = vfs_lookup(cwd, path, len, &next);

        if (ret)
            goto release_cwd;

        inode_put(cwd);
        cwd = next;

        path += len;
        if (*path == '/')
            path++;
    }

    *result = cwd;
    return 0;

  release_cwd:
    inode_put(cwd);
    return ret;
}

int namei(const char *path, struct inode **result)
{
    return namex(path, ino_root, result);
}

