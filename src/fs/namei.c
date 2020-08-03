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

#include <protura/block/bdev.h>
#include <protura/block/bcache.h>
#include <protura/fs/super.h>
#include <protura/fs/file.h>
#include <protura/fs/inode.h>
#include <protura/fs/namei.h>
#include <protura/fs/fs.h>

int namei_full(struct nameidata *data, flags_t flags)
{
    int link_count = 0;
    struct inode *cwd;
    const char *path;
    int ret = 0;

    path = data->path;
    cwd = data->cwd;

    if (!path)
        return -EFAULT;

    data->found = NULL;

    if (!cwd || *path == '/') {
        cwd = ino_root;
        path++;
    }

    cwd = inode_dup(cwd);

    while (*path) {
        struct inode *next;
        struct inode *link;
        struct inode *mount_point;
        size_t len = 0, old_len = 0;
        const char *old_path;

        if (*path == '/')
            path++;

        while (path[len] && path[len] != '/')
            len++;

        old_path = path;
        old_len = len;

        if (!path[len]) {
            if (flag_test(&flags, NAMEI_GET_PARENT))
                data->parent = inode_dup(cwd);

            data->name_start = path;
            data->name_len = len;
        }

        next = NULL;

        ret = vfs_lookup(cwd, path, len, &next);

        if (ret) {
            if (next)
                inode_put(next);
            goto release_cwd;
        }

      translate_inode:

        mount_point = vfs_get_mount(next);

        if (mount_point) {
            inode_put(next);
            next = mount_point;
            goto translate_inode;
        }

        if (!flag_test(&flags, NAMEI_DONT_FOLLOW_LINK) && S_ISLNK(next->mode)) {
            if (link_count == CONFIG_LINK_MAX) {
                ret = -ELOOP;
                inode_put(next);
                goto release_cwd;
            }

            link_count++;

            ret = vfs_follow_link(cwd, next, &link);

            if (ret) {
                inode_put(next);
                goto release_cwd;
            }

            inode_put(next);
            next = link;

            goto translate_inode;
        }

        path += len;

        if (flag_test(&flags, NAMEI_ALLOW_TRAILING_SLASH) && *path == '/' && !*(path + 1)) {
            if (flag_test(&flags, NAMEI_GET_PARENT))
                data->parent = inode_dup(cwd);

            data->name_start = old_path;
            data->name_len = old_len;

            path++;
        }

        inode_put(cwd);
        cwd = next;
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


