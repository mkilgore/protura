/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/list.h>
#include <protura/hlist.h>
#include <protura/string.h>
#include <arch/spinlock.h>
#include <protura/atomic.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/user_ptr.h>
#include <arch/task.h>

#include <protura/fs/block.h>
#include <protura/fs/super.h>
#include <protura/fs/file.h>
#include <protura/fs/stat.h>
#include <protura/fs/fcntl.h>
#include <protura/fs/inode.h>
#include <protura/fs/inode_table.h>
#include <protura/fs/namei.h>
#include <protura/fs/vfs.h>
#include <protura/fs/sys.h>

/* These functions connect the 'vfs_*' functions to the syscall verisons. Note
 * that these 'sys_*' functions are responsable for checking the userspace
 * pointers. */

int __sys_open(struct inode *inode, unsigned int file_flags, struct file **filp)
{
    int ret = 0;
    int fd;

    if (file_flags & F(FILE_WRITABLE) && S_ISDIR(inode->mode))
        return -EISDIR;

    fd = fd_get_empty();

    if (fd == -1)
        return -ENFILE;

    ret = vfs_open(inode, file_flags, filp);

    if (ret < 0)
        goto ret_fd_release;

    fd_assign(fd, *filp);
    return fd;

  ret_fd_release:
    fd_release(fd);
    return ret;
}

int sys_open(const char *__user path, int flags, mode_t mode)
{
    int ret;
    unsigned int file_flags = 0;
    struct inode *inode;
    struct file *filp;

    ret = user_check_strn(path, PATH_MAX, F(VM_MAP_READ));
    if (ret)
        return ret;

    if (flags & O_RDWR)
        file_flags = F(FILE_READABLE) | F(FILE_WRITABLE);
    else if (flags & O_WRONLY)
        file_flags = F(FILE_WRITABLE);
    else
        file_flags = F(FILE_READABLE);

    ret = namei(path, &inode);
    if (ret)
        goto return_result;

    ret = __sys_open(inode, file_flags, &filp);

    inode_put(inode);

  return_result:
    return ret;
}

int sys_close(int fd)
{
    struct file *filp;
    int ret;

    ret = fd_get_checked(fd, &filp);

    if (ret)
        return ret;

    fd_release(fd);

    return vfs_close(filp);
}

int sys_read(int fd, void *__user buf, size_t len)
{
    struct file *filp;
    int ret;

    ret = user_check_region(buf, len, F(VM_MAP_WRITE));
    if (ret)
        return ret;

    ret = fd_get_checked(fd, &filp);

    if (ret)
        return ret;

    return vfs_read(filp, buf, len);
}

int sys_read_dent(int fd, struct dent *__user dent, size_t size)
{
    struct file *filp;
    int ret;

    ret = user_check_region(dent, size, F(VM_MAP_WRITE));
    if (ret)
        return ret;

    ret = fd_get_checked(fd, &filp);

    if (ret)
        return ret;

    return vfs_read_dent(filp, dent, size);
}

int sys_write(int fd, void *__user buf, size_t len)
{
    struct file *filp;
    int ret;

    ret = user_check_region(buf, len, F(VM_MAP_READ));
    if (ret)
        return ret;

    ret = fd_get_checked(fd, &filp);

    if (ret)
        return ret;

    return vfs_write(filp, buf, len);
}

off_t sys_lseek(int fd, off_t off, int whence)
{
    struct file *filp;
    int ret;

    ret = fd_get_checked(fd, &filp);

    if (ret)
        return ret;

    return vfs_lseek(filp, off, whence);
}

