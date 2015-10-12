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
#include <protura/errors.h>
#include <protura/atomic.h>
#include <protura/semaphore.h>
#include <fs/dirent.h>

struct inode;
struct file_ops;

struct file {
    struct inode *inode;
    mode_t mode;
    atomic_t ref;
    mutex_t lock;

    off_t offset;

    struct file_ops *ops;
};

struct file_ops {
    int (*read) (struct file *, void *, size_t);
    int (*readdir) (struct file *, struct dirent *);
    off_t (*lseek) (struct file *, off_t offset, int whence);
    int (*write) (struct file *, void *, size_t);
};

enum file_whence {
    SEEK_SET,
    SEEK_CUR,
    SEEK_END,
};

#include <fs/inode.h>

void file_init(struct file *);
void file_clear(struct file *);

int file_fd_get(int fd, struct file **);
int file_fd_add(int fd, struct file *);
int file_fd_del(int fd);

struct file *file_dup(struct file *);

static inline int file_read(struct file *filp, void *buf, size_t len)
{
    if (filp->ops && filp->ops->read)
        return (filp->ops->read) (filp, buf, len);
    else
        return -ENOTSUP;
}

static inline int file_readdir(struct file *filp, struct dirent *ent)
{
    if (filp->ops && filp->ops->readdir)
        return (filp->ops->readdir) (filp, ent);
    else
        return -ENOTSUP;
}

static inline int file_write(struct file *filp, void *buf, size_t len)
{
    if (filp->ops && filp->ops->write)
        return (filp->ops->write) (filp, buf, len);
    else
        return -ENOTSUP;
}

static inline off_t file_lseek(struct file *filp, off_t off, int whence)
{
    if (filp->ops && filp->ops->lseek)
        return (filp->ops->lseek) (filp, off, whence);
    else
        return -ENOTSUP;
}

int fs_file_generic_read(struct file *, void *buf, size_t len);
int fs_file_generic_write(struct file *, void *buf, size_t len);
off_t fs_file_generic_lseek(struct file *, off_t off, int whence);

int sys_read(int fd, void *buf, size_t len);
int sys_write(int fd, void *buf, size_t len);
off_t sys_lseek(int fd, off_t off, int whence);

#endif
