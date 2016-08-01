/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_NAMEI_H
#define INCLUDE_FS_NAMEI_H

#include <protura/fs/inode.h>

struct nameidata {
    const char *path;
    struct inode *cwd;

    struct inode *found;
    struct inode *parent;
    const char *name_start;
    size_t name_len;
};

enum {
    NAMEI_GET_INODE,
    NAMEI_GET_PARENT,
};

int namei_full(struct nameidata *data, flags_t flags);

int namei(const char *path, struct inode **result);
int namex(const char *path, struct inode *cwd, struct inode **result);
/*
int nameiparent(const char *path, const char **name_start, size_t *name_len, struct inode **result);
int namexparent(const char *path, const char **name_start, size_t *name_len, struct inode *cwd, struct inode **result);
*/

#endif
