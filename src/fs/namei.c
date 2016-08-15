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

    kp(KP_TRACE, "data->cwd: %p\n", data->cwd);

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

        if (flag_test(&next->flags, INO_MOUNT)) {
            kp(KP_TRACE, "Translating mount-point\n");
            link = NULL;

            using_inode_mount(next) {
                kp(KP_TRACE, "next->mount: %p\n", next->mount);
                if (next->mount)
                    link = inode_dup(next->mount);
            }

            kp(KP_TRACE, "Translating mount-point done\n");
            if (link) {
                inode_put(next);
                next = link;
                goto translate_inode;
            }
        }

        if (!flag_test(&flags, NAMEI_DONT_FOLLOW_LINK) && S_ISLNK(next->mode)) {
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


