/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_VFS_H
#define INCLUDE_FS_VFS_H

#include <protura/fs/inode.h>
#include <protura/fs/file.h>
#include <protura/fs/dent.h>

/* Note: vfs_open and vfs_close do *not* touch the file-descriptor table for
 * the current process. To do that, use sys_*.
 *
 * As a note: __sys_open is similar to vfs_open, but will also assign a fd for
 * you. Useful if you want to avoid the overhead of namei because you already
 * have the inode. */
int vfs_open(struct inode *inode, unsigned int file_flags, struct file **filp);
int vfs_close(struct file *filp);
int vfs_read(struct file *filp, void *buf, size_t len);
int vfs_write(struct file *filp, void *buf, size_t len);
off_t vfs_lseek(struct file *filp, off_t off, int whence);
int vfs_read_dent(struct file *filp, struct dent *, size_t size);

int vfs_lookup(struct inode *inode, const char *name, size_t len, struct inode **result);
int vfs_truncate(struct inode *inode, off_t length);
sector_t vfs_bmap(struct inode *inode, sector_t);

/* If the provided inode doesn't support 'bmap_alloc', this will fall back to
 * calling the regular 'bmap' automatically */
sector_t vfs_bmap_alloc(struct inode *inode, sector_t);

int vfs_link(struct inode *dir, struct inode *old, const char *name, size_t len);

int vfs_chdir(const char *path);

int vfs_getdents(struct file *filp, struct dent *, size_t dent_buf_size);

#endif
