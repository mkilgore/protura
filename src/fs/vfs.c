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


int vfs_open(struct inode *inode, unsigned int file_flags, struct file **filp_ret)
{
    int ret = 0;
    struct file *filp;

    kp(KP_TRACE, "Opening file: %p, %d, %p\n", inode, file_flags, filp_ret);

    *filp_ret = NULL;

    filp = kzalloc(sizeof(*filp), PAL_KERNEL);

    kp(KP_TRACE, "Allocated filp: %p\n", filp);

    filp->mode = inode->mode;
    filp->inode = inode_dup(inode);
    filp->offset = 0;
    filp->flags = file_flags;
    filp->ops = inode->default_fops;

    atomic_inc(&filp->ref);

    if (file_has_open(filp))
        ret = filp->ops->open(inode, filp);

    if (ret < 0)
        goto cleanup_filp;

    *filp_ret = filp;

    return ret;

  cleanup_filp:
    inode_put(filp->inode);
    kfree(filp);

    return ret;
}

int vfs_close(struct file *filp)
{
    int ret = 0;

    kp(KP_TRACE, "closing file, inode:"PRinode", %d\n", Pinode(filp->inode), atomic_get(&filp->ref));

    if (!atomic_dec_and_test(&filp->ref))
        return 0;

    kp(KP_TRACE, "Releasing file with inode:"PRinode"!\n", Pinode(filp->inode));

    if (file_has_release(filp))
        ret = filp->ops->release(filp);

    inode_put(filp->inode);

    kp(KP_TRACE, "Freeing file %p\n", filp);
    kfree(filp);

    return ret;
}

int vfs_read(struct file *filp, void *buf, size_t len)
{
    if (S_ISDIR(filp->inode->mode))
        return -EISDIR;

    if (file_has_read(filp))
        return filp->ops->read(filp, buf, len);
    else
        return -ENOTSUP;
}

int vfs_read_dent(struct file *filp, struct dent *dent, size_t size)
{
    if (!S_ISDIR(filp->inode->mode))
        return -ENOTDIR;

    if (file_has_read_dent(filp))
        return filp->ops->read_dent(filp, dent, size);
    else
        return -ENOTSUP;
}

int vfs_write(struct file *filp, void *buf, size_t len)
{
    if (S_ISDIR(filp->inode->mode))
        return -EISDIR;

    if (file_has_write(filp))
        return filp->ops->write(filp, buf, len);
    else
        return -ENOTSUP;
}

off_t vfs_lseek(struct file *filp, off_t off, int whence)
{
    if (S_ISDIR(filp->inode->mode))
        return -EISDIR;

    if (file_has_lseek(filp))
        return filp->ops->lseek(filp, off, whence);
    else
        return -ENOTSUP;
}

int vfs_lookup(struct inode *inode, const char *name, size_t len, struct inode **result)
{
    if (!S_ISDIR(inode->mode))
        return -ENOTDIR;

    if (inode_has_lookup(inode))
        return inode->ops->lookup(inode, name, len, result);
    else
        return -ENOTSUP;
}

int vfs_truncate(struct inode *inode, off_t length)
{
    if (S_ISDIR(inode->mode))
        return -EISDIR;

    if (inode_has_truncate(inode))
        return inode->ops->truncate(inode, length);
    else
        return -ENOTSUP;
}

sector_t vfs_bmap(struct inode *inode, sector_t s)
{
    if (inode_has_bmap(inode))
        return inode->ops->bmap(inode, s);
    else
        return -ENOTSUP;
}

sector_t vfs_bmap_alloc(struct inode *inode, sector_t s)
{
    if (inode_has_bmap_alloc(inode))
        return inode->ops->bmap_alloc(inode, s);
    else
        return vfs_bmap(inode, s);
}

int vfs_link(struct inode *dir, struct inode *old, const char *name, size_t len)
{
    if (!S_ISDIR(dir->mode))
        return -ENOTDIR;

    if (inode_has_link(dir))
        return dir->ops->link(dir, old, name, len);
    else
        return -ENOTSUP;
}

int vfs_mknod(struct inode *dir, const char *name, size_t len, mode_t mode, dev_t dev)
{
    if (!S_ISDIR(dir->mode))
        return -ENOTDIR;

    if (inode_has_mknod(dir))
        return dir->ops->mknod(dir, name, len, mode, dev);
    else
        return -ENOTSUP;
}

int vfs_unlink(struct inode *dir, struct inode *entity, const char *name, size_t len)
{
    if (!S_ISDIR(dir->mode))
        return -ENOTDIR;

    if (inode_has_unlink(dir))
        return dir->ops->unlink(dir, entity, name, len);
    else
        return -ENOTSUP;
}

int vfs_chdir(const char *path)
{
    struct task *current = cpu_get_local()->current;
    struct nameidata name;
    int ret;

    kp(KP_TRACE, "chdir: %s\n", path);

    memset(&name, 0, sizeof(name));

    name.path = path;
    name.cwd = current->cwd;

    ret = namei_full(&name, F(NAMEI_GET_INODE) | F(NAMEI_ALLOW_TRAILING_SLASH));
    if (!name.found)
        return ret;

    inode_put(current->cwd);
    current->cwd = name.found;

    return 0;
}

int vfs_stat(struct inode *inode, struct stat *buf)
{
    buf->st_dev = DEV_TO_USERSPACE(inode->sb_dev);
    buf->st_ino = inode->ino;
    buf->st_mode = inode->mode;
    buf->st_nlink = atomic32_get(&inode->nlinks);
    buf->st_size = inode->size;
    buf->st_rdev = DEV_TO_USERSPACE(inode->dev_no);

    buf->st_uid = 0;
    buf->st_gid = 0;

    buf->st_atime = 0;
    buf->st_ctime = 0;
    buf->st_mtime = 0;
    buf->st_blksize = inode->block_size;
    buf->st_blocks = inode->blocks;

    return 0;
}

int vfs_create(struct inode *dir, const char *name, size_t len, mode_t mode, struct inode **result)
{
    if (!S_ISDIR(dir->mode))
        return -ENOTDIR;

    if (inode_has_create(dir))
        return dir->ops->create(dir, name, len, mode, result);
    else
        return -ENOTSUP;
}

int vfs_mkdir(struct inode *dir, const char *name, size_t len, mode_t mode)
{
    if (!S_ISDIR(dir->mode))
        return -ENOTDIR;

    if (inode_has_mkdir(dir))
        return dir->ops->mkdir(dir, name, len, mode);
    else
        return -ENOTSUP;
}

int vfs_rmdir(struct inode *dir, struct inode *deldir, const char *name, size_t len)
{
    if (!S_ISDIR(dir->mode))
        return -ENOTDIR;

    if (inode_has_rmdir(dir))
        return dir->ops->rmdir(dir, deldir, name, len);
    else
        return -ENOTSUP;
}

int vfs_rename(struct inode *old_dir, const char *name, size_t len, struct inode *new_dir, const char *new_name, size_t new_len)
{
    if (!S_ISDIR(old_dir->mode))
        return -ENOTDIR;

    if (inode_has_rename(old_dir))
        return old_dir->ops->rename(old_dir, name, len, new_dir, new_name, new_len);
    else
        return -ENOTSUP;
}

int vfs_follow_link(struct inode *dir, struct inode *symlink, struct inode **result)
{
    if (inode_has_follow_link(symlink))
        return symlink->ops->follow_link(dir, symlink, result);
    else
        return -ENOTSUP;
}

int vfs_readlink(struct inode *symlink, char *buf, size_t buf_len)
{
    if (inode_has_readlink(symlink))
        return symlink->ops->readlink(symlink, buf, buf_len);
    else
        return -ENOTSUP;
}

int vfs_symlink(struct inode *dir, const char *name, size_t len, const char *symlink_target)
{
    if (!S_ISDIR(dir->mode))
        return -ENOTDIR;

    if (inode_has_symlink(dir))
        return dir->ops->symlink(dir, name, len, symlink_target);
    else
        return -ENOTSUP;
}

