/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_SYS_H
#define INCLUDE_FS_SYS_H

#include <protura/irq.h>
#include <protura/fs/stat.h>
#include <protura/fs/inode.h>
#include <protura/fs/file.h>
#include <protura/fs/sync.h>

#define PATH_MAX 256

int sys_open(const struct user_buffer file, int flags, mode_t mode);
int sys_close(int fd);
int sys_read(int fd, struct user_buffer buf, size_t len);
int sys_write(int fd, const struct user_buffer buf, size_t len);
off_t sys_lseek(int fd, off_t off, int whence);
int sys_read_dent(int fd, struct user_buffer dent, size_t size);
int sys_pipe(struct user_buffer fds);

int sys_truncate(struct user_buffer file, off_t length);
int sys_ftruncate(int fd, off_t length);
int sys_link(struct user_buffer old, struct user_buffer new);
int sys_unlink(struct user_buffer name);
int sys_chdir(struct user_buffer path);
int sys_stat(struct user_buffer path, struct user_buffer buf);
int sys_fstat(int fd, struct user_buffer buf);
int sys_mkdir(struct user_buffer name, mode_t mode);
int sys_mknod(struct user_buffer node, mode_t mode, dev_t dev);
int sys_rmdir(struct user_buffer name);
int sys_rename(struct user_buffer old, struct user_buffer new);
int sys_lstat(struct user_buffer path, struct user_buffer buf);
int sys_readlink(struct user_buffer path, struct user_buffer buf, size_t buf_len);
int sys_symlink(struct user_buffer target, struct user_buffer link);

/* Called internally by sys_open - Performs the same function, but takes
 * arguments relating to inode's and file's rather then a path name and
 * userspace flags. If you have to use sys_open in the kernel. */
int __sys_open(struct inode *inode, unsigned int file_flags, struct file **filp);

int sys_execve(struct user_buffer file_buf, struct user_buffer argv_buf, struct user_buffer envp_buf, struct irq_frame *frame);

int sys_mount(struct user_buffer source, struct user_buffer target, struct user_buffer fsystem, unsigned long mountflags, struct user_buffer data);
int sys_umount(struct user_buffer target_buf);

int sys_fcntl(int fd, int cmd, uintptr_t arg);

int sys_ioctl(int fd, int cmd, struct user_buffer arg);

int sys_chown(struct user_buffer path, uid_t uid, gid_t gid);
int sys_fchown(int fd, uid_t uid, gid_t gid);
int sys_lchown(struct user_buffer path, uid_t uid, gid_t gid);

int sys_chmod(struct user_buffer path, mode_t mode);
int sys_fchmod(int fd, mode_t mode);

mode_t sys_umask(mode_t);

int sys_access(struct user_buffer path_buf, int mode);
int sys_utimes(struct user_buffer path_buf, struct user_buffer timeval_buf);

#endif
