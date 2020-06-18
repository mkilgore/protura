/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/task.h>

#include <protura/fs/stat.h>
#include <protura/fs/inode.h>
#include <protura/fs/sys.h>

/* Verifies that the provided credentials can access the mode according to the
 * provided access flags.
 *
 * The credentials should be locked before calling this function. */
int __check_permission(struct credentials *creds, struct inode *inode, int access)
{
    if (!access)
        return 0;

    mode_t mode = inode->mode & 0777;

    if (creds->euid == 0) {
        /* Root always has access, but for regular files X_OK is only valid if
         * at least one of the execute bits are set */
        if (!S_ISREG(mode))
            return 0;

        if (!(mode & X_OK))
            return 0;

        if (mode & (S_IXUSR | S_IXGRP | S_IXOTH))
            return 0;

        return -EACCES;
    }

    /* Select the appropriate set of mode bits for the credentials */
    if (creds->euid == inode->uid)
        mode >>= 6;
    else if (__credentials_belong_to_gid(creds, inode->gid))
        mode >>= 3;

    if ((mode & access) == access)
        return 0;

    return -EACCES;
}

int check_permission(struct inode *inode, int access)
{
    struct task *current = cpu_get_local()->current;
    using_creds(&current->creds)
        return __check_permission(&current->creds, inode, access);
}
