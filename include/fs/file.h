/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_FILE_H
#define INCLUDE_FS_FILE_H

#include <protura/types.h>
#include <fs/inode.h>
#include <fs/vfsnode.h>

enum lseek_whence {
    SEEK_SET,
    SEEK_CUR,
    SEEK_END,
};

struct dirent {
    ino_t d_ino;
    char d_name[256];
};

struct file_ops;

struct file {
    struct inode *inode;
    struct vfsnode *vfsnode;
    off_t pos;
    struct file_ops *ops;
};

struct file_ops {
    off_t (*lseek) (struct file *, off_t offset, enum lseek_whence whence);
    int (*read) (struct file *, char *buf, size_t size);
    int (*write) (struct file *, char *buf, size_t size);
    int (*open) (struct file *, struct inode *);
    int (*release) (struct file *);
};

#endif
