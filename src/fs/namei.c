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
#include <mm/kmalloc.h>

#include <fs/block.h>
#include <fs/super.h>
#include <fs/file.h>
#include <fs/inode.h>
#include <fs/inode_table.h>
#include <fs/namei.h>
#include <fs/fs.h>

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

        kprintf("namex len: %d\n", len);

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

