/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_VFS_H
#define INCLUDE_FS_VFS_H

#include <protura/fs/stat.h>
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
int vfs_open_noalloc(struct inode *inode, unsigned int file_flags, struct file *filp);
int vfs_close(struct file *filp);
int vfs_pread(struct file *filp, void *buf, size_t len, off_t off);
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
int vfs_unlink(struct inode *dir, struct inode *entity, const char *name, size_t len);

int vfs_chdir(const char *path);

int vfs_getdents(struct file *filp, struct dent *, size_t dent_buf_size);

int vfs_stat(struct inode *inode, struct stat *buf);

int vfs_create(struct inode *dir, const char *name, size_t len, mode_t mode, struct inode **result);
int vfs_mkdir(struct inode *dir, const char *name, size_t len, mode_t mode);
int vfs_rmdir(struct inode *dir, struct inode *deldir, const char *name, size_t len);
int vfs_mknod(struct inode *dir, const char *name, size_t len, mode_t mode, dev_t dev);
int vfs_rename(struct inode *old_dir, const char *name, size_t len, struct inode *new_dir, const char *new_name, size_t new_len);
int vfs_follow_link(struct inode *dir, struct inode *symlink, struct inode **result);
int vfs_readlink(struct inode *symlink, char *buf, size_t buf_len);
int vfs_symlink(struct inode *dir, const char *name, size_t len, const char *symlink_target);

/* devname is for user convience when display mount points and can be left NULL */
int vfs_mount(struct inode *mount_point, dev_t block_dev, const char *filesystem, const char *devname, const char *mountname);
int vfs_umount(struct super_block *sb);

#endif
