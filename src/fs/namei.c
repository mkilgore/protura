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

int namei_full(struct nameidata *data, flags_t flags)
{
    struct inode *cwd;
    const char *path;
    int ret = 0;

    path = data->path;
    cwd = data->cwd;

    if (!path)
        return -EFAULT;

    data->found = NULL;

    if (!cwd || *path == '/')
        cwd = ino_root;

    cwd = inode_dup(cwd);

    while (*path) {
        struct inode *next;
        size_t len = 0;

        if (*path == '/')
            path++;

        while (path[len] && path[len] != '/')
            len++;

        if (!path[len]) {
            if (flag_test(&flags, NAMEI_GET_PARENT))
                data->parent = inode_dup(cwd);

            data->name_start = path;
            data->name_len = len;
        }

        ret = vfs_lookup(cwd, path, len, &next);

        if (ret)
            goto release_cwd;

        inode_put(cwd);
        cwd = next;

        path += len;
    }

    if (flag_test(&flags, NAMEI_GET_INODE))
        data->found = cwd;
    else
        inode_put(cwd);
    return 0;

  release_cwd:
    inode_put(cwd);
    return ret;
}

#if 0
static int namei_generic(const char *path, const char **name_start, size_t *name_len, int get_parent, struct inode *cwd, struct inode **result)
{
    int ret = 0;

    if (!path)
        return -EFAULT;

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

        if (!path[len] && get_parent) {
            *name_start = path;
            *name_len = len;
            break;
        }

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

int namexparent(const char *path, const char **name_start, size_t *name_len, struct inode *cwd, struct inode **result)
{
    return namei_generic(path, name_start, name_len, 1, cwd, result);
}

int nameiparent(const char *path, const char **name_start, size_t *name_len, struct inode **result)
{
    return namexparent(path, name_start, name_len, ino_root, result);
}
#endif

int namex(const char *path, struct inode *cwd, struct inode **result)
{
    struct nameidata name;
    int ret;

    name.path = path;
    name.cwd = cwd;

    ret = namei_full(&name, F(NAMEI_GET_INODE));

    *result = name.found;

    return ret;
}

int namei(const char *path, struct inode **result)
{
    return namex(path, ino_root, result);
}


