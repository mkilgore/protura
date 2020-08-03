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
#include <arch/task.h>

#include <protura/block/bdev.h>
#include <protura/block/bcache.h>
#include <protura/fs/super.h>
#include <protura/fs/file.h>
#include <protura/fs/stat.h>
#include <protura/fs/fcntl.h>
#include <protura/fs/inode.h>
#include <protura/fs/namei.h>
#include <protura/fs/sys.h>
#include <protura/fs/access.h>
#include <protura/fs/vfs.h>

#ifdef CONFIG_KERNEL_LOG_VFS
# define kp_vfs(str, ...) kp(KP_DEBUG, "vfs: " str, ## __VA_ARGS__)
#else
# define kp_vfs(str, ...) do { ; } while (0)
#endif


int vfs_open_noalloc(struct inode *inode, unsigned int file_flags, struct file *filp)
{
    int ret = 0;

    int access = 0;

    if (flag_test(&file_flags, FILE_READABLE))
        access |= R_OK;

    if (flag_test(&file_flags, FILE_WRITABLE))
        access |= W_OK;

    ret = check_permission(inode, access);
    if (ret)
        return ret;

    kp_vfs("Allocated filp: %p\n", filp);

    filp->inode = inode_dup(inode);
    filp->offset = 0;
    filp->flags = file_flags;
    filp->ops = inode->default_fops;

    atomic_inc(&filp->ref);

    if (file_has_open(filp))
        ret = filp->ops->open(inode, filp);

    if (ret < 0)
        goto cleanup_filp;

    return ret;

  cleanup_filp:
    inode_put(filp->inode);

    return ret;
}

int vfs_open(struct inode *inode, unsigned int file_flags, struct file **filp_ret)
{
    int ret = 0;
    struct file *filp;

    kp_vfs("Opening file: %p, %d, %p\n", inode, file_flags, filp_ret);

    *filp_ret = NULL;

    filp = kzalloc(sizeof(*filp), PAL_KERNEL);

    ret = vfs_open_noalloc(inode, file_flags, filp);

    if (!ret)
        *filp_ret = filp;
    else
        kfree(filp);

    return ret;
}

int vfs_close(struct file *filp)
{
    int ret = 0;

    kp_vfs("closing file, inode:"PRinode", %d\n", Pinode(filp->inode), atomic_get(&filp->ref));

    if (!atomic_dec_and_test(&filp->ref))
        return 0;

    kp_vfs("Releasing file with inode:"PRinode"!\n", Pinode(filp->inode));

    if (file_has_release(filp))
        ret = filp->ops->release(filp);

    inode_put(filp->inode);

    kp_vfs("Freeing file %p\n", filp);
    kfree(filp);

    return ret;
}

int vfs_read(struct file *filp, struct user_buffer buf, size_t len)
{
    if (S_ISDIR(filp->inode->mode))
        return -EISDIR;

    if (file_has_read(filp))
        return filp->ops->read(filp, buf, len);
    else
        return -ENOTSUP;
}

int vfs_pread(struct file *filp, struct user_buffer buf, size_t len, off_t off)
{
    if (S_ISDIR(filp->inode->mode))
        return -EISDIR;

    if (file_has_pread(filp))
        return filp->ops->pread(filp, buf, len, off);
    else
        return -ENOTSUP;
}

int vfs_read_dent(struct file *filp, struct user_buffer dent, size_t size)
{
    if (!S_ISDIR(filp->inode->mode))
        return -ENOTDIR;

    if (file_has_read_dent(filp))
        return filp->ops->read_dent(filp, dent, size);
    else
        return -ENOTSUP;
}

int vfs_write(struct file *filp, struct user_buffer buf, size_t len)
{
    if (S_ISDIR(filp->inode->mode))
        return -EISDIR;

    if (file_has_write(filp)) {
        if (S_ISREG(filp->inode->mode)) {
            int ret = vfs_apply_attributes(filp->inode, F(INODE_ATTR_RM_SGID, INODE_ATTR_RM_SUID, INODE_ATTR_FORCE), NULL);
            if (ret)
                return ret;
        }

        return filp->ops->write(filp, buf, len);
    } else {
        return -ENOTSUP;
    }
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

    int ret = check_permission(inode, X_OK);
    if (ret)
        return ret;

    if (inode_has_lookup(inode))
        return inode->ops->lookup(inode, name, len, result);
    else
        return -ENOTSUP;
}

int vfs_truncate(struct inode *inode, off_t length)
{
    if (S_ISDIR(inode->mode))
        return -EISDIR;

    int ret = check_permission(inode, W_OK);
    if (ret)
        return ret;

    if (inode_has_truncate(inode)) {
        if (S_ISREG(inode->mode)) {
            int ret = vfs_apply_attributes(inode, F(INODE_ATTR_RM_SGID, INODE_ATTR_RM_SUID, INODE_ATTR_FORCE), NULL);
            if (ret)
                return ret;
        }

        return inode->ops->truncate(inode, length);
    } else {
        return -ENOTSUP;
    }
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

    int ret = check_permission(dir, W_OK | X_OK);
    if (ret)
        return ret;

    if (inode_has_link(dir))
        return dir->ops->link(dir, old, name, len);
    else
        return -ENOTSUP;
}

int vfs_mknod(struct inode *dir, const char *name, size_t len, mode_t mode, dev_t dev)
{
    if (!S_ISDIR(dir->mode))
        return -ENOTDIR;

    int ret = check_permission(dir, W_OK | X_OK);
    if (ret)
        return ret;

    if (inode_has_mknod(dir))
        return dir->ops->mknod(dir, name, len, mode, dev);
    else
        return -ENOTSUP;
}

int vfs_unlink(struct inode *dir, struct inode *entity, const char *name, size_t len)
{
    if (!S_ISDIR(dir->mode))
        return -ENOTDIR;

    int ret = check_permission(dir, W_OK | X_OK);
    if (ret)
        return ret;

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

    kp_vfs("chdir: %s\n", path);

    memset(&name, 0, sizeof(name));

    name.path = path;
    name.cwd = current->cwd;

    ret = namei_full(&name, F(NAMEI_GET_INODE) | F(NAMEI_ALLOW_TRAILING_SLASH));
    if (!name.found)
        return ret;

    if (!S_ISDIR(name.found->mode)) {
        inode_put(name.found);
        return -ENOTDIR;
    }

    inode_put(current->cwd);
    current->cwd = name.found;

    return 0;
}

int vfs_stat(struct inode *inode, struct stat *buf)
{
    buf->st_dev = inode->sb->bdev->dev;
    buf->st_ino = inode->ino;
    buf->st_mode = inode->mode;
    buf->st_nlink = atomic32_get(&inode->nlinks);
    buf->st_size = inode->size;
    buf->st_rdev = inode->dev_no;

    buf->st_uid = inode->uid;
    buf->st_gid = inode->gid;

    buf->st_atime = inode->atime;
    buf->st_ctime = inode->ctime;
    buf->st_mtime = inode->mtime;
    buf->st_blksize = inode->block_size;
    buf->st_blocks = inode->blocks;

    return 0;
}

static int verify_apply_attribute_permissions(struct inode *inode, flags_t flags, struct inode_attributes *attrs)
{
    struct task *current = cpu_get_local()->current;

    if (flag_test(&flags, INODE_ATTR_FORCE))
        return 0;

    using_creds(&current->creds) {
        struct credentials *creds = &current->creds;

        if (flag_test(&flags, INODE_ATTR_UID)) {
            /* changing UID is only allowed if you're root, or if the change is a NO-OP */
            int is_valid = creds->euid == 0 || (creds->euid == inode->uid && inode->uid == attrs->uid);

            if (!is_valid)
                return -EPERM;
        }

        if (flag_test(&flags, INODE_ATTR_GID)) {
            /* changing GID is only allowed if you're root or if you own the file and belong to the destination group */
            int is_valid = creds->euid == 0
                           || (creds->euid == inode->uid
                                   && (__credentials_belong_to_gid(creds, attrs->gid) || attrs->gid == inode->gid));

            if (!is_valid)
                return -EPERM;
        }

        if (flag_test(&flags, INODE_ATTR_MODE)) {
            /* UID must match or root for chmod to be allowed */
            if (creds->euid != 0 && creds->euid != inode->uid)
                return -EPERM;

            /* Clear SGID bit if you do not belong to the target GID */
            gid_t target = flag_test(&flags, INODE_ATTR_GID)? attrs->gid: inode->gid;

            if (creds->euid != 0 && !__credentials_belong_to_gid(creds, target))
                attrs->mode &= ~S_ISGID;
        }

        /* You must own the file to manually change the time */
        if (flag_test(&flags, INODE_ATTR_ATIME) || flag_test(&flags, INODE_ATTR_MTIME) || flag_test(&flags, INODE_ATTR_CTIME))
            if (creds->euid != 0 || creds->euid != inode->uid)
                return -EPERM;
    }

    return 0;
}

int vfs_apply_attributes(struct inode *inode, flags_t flags, struct inode_attributes *attrs)
{
    struct inode_attributes tmp_attrs;

    if (!attrs) {
        memset(&tmp_attrs, 0, sizeof(tmp_attrs));
        attrs = &tmp_attrs;
    }

    using_inode_lock_write(inode) {
        if ((flag_test(&flags, INODE_ATTR_RM_SGID) && flag_test(&flags, INODE_ATTR_MODE))
            || (flag_test(&flags, INODE_ATTR_RM_SUID) && flag_test(&flags, INODE_ATTR_MODE)))
            return -EINVAL;

        if (flag_test(&flags, INODE_ATTR_RM_SUID) || flag_test(&flags, INODE_ATTR_RM_SGID)) {
            attrs->mode = inode->mode;

            if (flag_test(&flags, INODE_ATTR_RM_SUID))
                attrs->mode = attrs->mode & ~S_ISUID;

            if (flag_test(&flags, INODE_ATTR_RM_SGID))
                attrs->mode = attrs->mode & ~S_ISGID;

            flag_set(&flags, INODE_ATTR_MODE);
        }

        if (verify_apply_attribute_permissions(inode, flags, attrs))
            return -EPERM;

        if (flag_test(&flags, INODE_ATTR_MODE)) {
            /* Make sure to clear the non-relevant bits of the mode, just in case */
            inode->mode = (inode->mode & S_IFMT) | (attrs->mode & 07777);
        }

        if (flag_test(&flags, INODE_ATTR_UID))
            inode->uid = attrs->uid;

        if (flag_test(&flags, INODE_ATTR_GID))
            inode->gid = attrs->gid;

        time_t cur_time = protura_current_time_get();

        if (flag_test(&flags, INODE_ATTR_ATIME))
            inode->atime = attrs->atime;
        else
            inode->atime = cur_time;

        if (flag_test(&flags, INODE_ATTR_CTIME))
            inode->ctime = attrs->ctime;
        else
            inode->ctime = cur_time;

        if (flag_test(&flags, INODE_ATTR_MTIME))
            inode->mtime = attrs->mtime;
        else
            inode->mtime = cur_time;

        inode_set_dirty(inode);
    }

    return 0;
}

int vfs_create(struct inode *dir, const char *name, size_t len, mode_t mode, struct inode **result)
{
    if (!S_ISDIR(dir->mode))
        return -ENOTDIR;

    int ret = check_permission(dir, W_OK | X_OK);
    if (ret)
        return ret;

    if (inode_has_create(dir))
        return dir->ops->create(dir, name, len, mode, result);
    else
        return -ENOTSUP;
}

int vfs_mkdir(struct inode *dir, const char *name, size_t len, mode_t mode)
{
    if (!S_ISDIR(dir->mode))
        return -ENOTDIR;

    int ret = check_permission(dir, W_OK | X_OK);
    if (ret)
        return ret;

    if (inode_has_mkdir(dir))
        return dir->ops->mkdir(dir, name, len, mode);
    else
        return -ENOTSUP;
}

int vfs_rmdir(struct inode *dir, struct inode *deldir, const char *name, size_t len)
{
    if (!S_ISDIR(dir->mode))
        return -ENOTDIR;

    int ret = check_permission(dir, W_OK | X_OK);
    if (ret)
        return ret;

    if (inode_has_rmdir(dir))
        return dir->ops->rmdir(dir, deldir, name, len);
    else
        return -ENOTSUP;
}

int vfs_rename(struct inode *old_dir, const char *name, size_t len, struct inode *new_dir, const char *new_name, size_t new_len)
{
    if (!S_ISDIR(old_dir->mode))
        return -ENOTDIR;

    int ret = check_permission(old_dir, W_OK | X_OK);
    if (ret)
        return ret;

    ret = check_permission(new_dir, W_OK | X_OK);
    if (ret)
        return ret;

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

    int ret = check_permission(dir, W_OK | X_OK);
    if (ret)
        return ret;

    if (inode_has_symlink(dir))
        return dir->ops->symlink(dir, name, len, symlink_target);
    else
        return -ENOTSUP;
}

int vfs_chown(struct inode *inode, uid_t uid, gid_t gid)
{
    struct inode_attributes attrs;
    memset(&attrs, 0, sizeof(attrs));

    flags_t flags = 0;

    if (uid != (uid_t)-1) {
        attrs.uid = uid;
        flag_set(&flags, INODE_ATTR_UID);
    }

    if (gid != (gid_t)-1) {
        attrs.gid = gid;
        flag_set(&flags, INODE_ATTR_GID);
    }

    if (!S_ISDIR(inode->mode)) {
        flag_set(&flags, INODE_ATTR_RM_SUID);
        flag_set(&flags, INODE_ATTR_RM_SGID);
    }

    return vfs_apply_attributes(inode, flags, &attrs);
}

int vfs_chmod(struct inode *inode, mode_t mode)
{
    struct inode_attributes attrs;
    memset(&attrs, 0, sizeof(attrs));

    attrs.mode = mode;

    return vfs_apply_attributes(inode, F(INODE_ATTR_MODE), &attrs);
}
