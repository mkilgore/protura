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
#include <protura/snprintf.h>
#include <arch/spinlock.h>
#include <protura/mutex.h>
#include <protura/atomic.h>
#include <protura/mm/kmalloc.h>
#include <arch/task.h>

#include <protura/fs/block.h>
#include <protura/fs/super.h>
#include <protura/fs/file.h>
#include <protura/fs/seq_file.h>
#include <protura/fs/stat.h>
#include <protura/fs/inode.h>
#include <protura/fs/inode_table.h>
#include <protura/fs/procfs.h>
#include <protura/fs/vfs.h>
#include <protura/fs/fs.h>

static mutex_t super_lock = MUTEX_INIT(super_lock, "super-block-list-lock");
static list_head_t super_block_list = LIST_HEAD_INIT(super_block_list);

struct vfs_mount {
    list_node_t mount_point_node;

    dev_t dev;
    char *devname;

    char *mountname;
    char *filesystem;
};

#define VFS_MOUNT_INIT(mount)\
    { \
        .mount_point_node = LIST_NODE_INIT((mount).mount_point_node), \
    }

static inline void vfs_mount_init(struct vfs_mount *mount)
{
    *mount = (struct vfs_mount)VFS_MOUNT_INIT(*mount);
}

static mutex_t mount_list_lock = MUTEX_INIT(mount_list_lock, "mount-list-lock");
static list_head_t mount_list = LIST_HEAD_INIT(mount_list);

static struct super_block *super_get(dev_t device, const char *filesystem)
{
    struct file_system *system;
    struct super_block *sb;

    system = file_system_lookup(filesystem);
    if (!system)
        return NULL;

    using_mutex(&super_lock) {
        if (!system->read_sb)
            return NULL;

        /* Don't allow mounting devices that are already mounted elsewhere */
        list_foreach_entry(&super_block_list, sb, list_entry)
            if (sb->dev == device)
                return NULL;

        sb = (system->read_sb) (device);
        if (!sb)
            return NULL;

        list_add(&super_block_list, &sb->list_entry);
    }

    return sb;
}

void __super_sync(struct super_block *sb)
{
    struct inode *inode;

    if (sb->bdev)
        block_dev_sync(sb->bdev, sb->dev, 0);

    if (sb->ops->inode_write)
        list_foreach_take_entry(&sb->dirty_inodes, inode, sb_dirty_entry)
            sb->ops->inode_write(sb, inode);

    if (sb->ops->sb_write)
        sb->ops->sb_write(sb);

    if (sb->bdev)
        block_dev_sync(sb->bdev, sb->dev, 1);
}

static int super_put(struct super_block *super)
{
    kp(KP_TRACE, "Super put\n");
    __super_sync(super);

    using_mutex(&super_lock)
        list_del(&super->list_entry);

    sb_put(super);

    return 0;
}

void super_sync(struct super_block *sb)
{
    using_super_block(sb)
        __super_sync(sb);
}

void sys_sync(void)
{
    struct super_block *sb;

    using_mutex(&super_lock)
        list_foreach_entry(&super_block_list, sb, list_entry)
            super_sync(sb);
}

static inline void mount_list_add(dev_t device, const char *devname, const char *mountname, const char *fs)
{
    struct vfs_mount *mount_rec;

    mount_rec = kmalloc(sizeof(*mount_rec), PAL_KERNEL);
    vfs_mount_init(mount_rec);

    mount_rec->dev = device;
    mount_rec->mountname = kstrdup(mountname, PAL_KERNEL);
    mount_rec->filesystem = kstrdup(fs, PAL_KERNEL);

    if (devname)
        mount_rec->devname = kstrdup(devname, PAL_KERNEL);

    using_mutex(&mount_list_lock)
        list_add_tail(&mount_list, &mount_rec->mount_point_node);

    return ;
}

static inline void mount_list_del(dev_t device)
{
    struct vfs_mount *mount, *found = NULL;

    using_mutex(&mount_list_lock) {
        list_foreach_entry(&mount_list, mount, mount_point_node) {
            if (mount->dev == device) {
                list_del(&mount->mount_point_node);
                found = mount;
                break;
            }
        }
    }

    if (found) {
        if (found->devname)
            kfree(found->devname);
        kfree(found->mountname);
        kfree(found->filesystem);
        kfree(found);
    }

}

int mount_root(dev_t device, const char *fsystem)
{
    struct super_block *sb = super_get(device, fsystem);
    if (!sb)
        return -EINVAL;

    kp(KP_NORMAL, "Mounting root...\n");
    ino_root = inode_dup(sb->root);

    mount_list_add(device, NULL, "/", fsystem);

    return 0;
}

static int super_mount(struct super_block *super, struct inode *mount_point)
{
    int ret = 0;

    using_inode_mount(mount_point) {
        if (!flag_test(&mount_point->flags, INO_MOUNT) && mount_point != super->root) {
            super->covered = inode_dup(mount_point);
            mount_point->mount = inode_dup(super->root);
            flag_set(&mount_point->flags, INO_MOUNT);
        } else {
            ret = -EBUSY;
        }
    }

    return ret;
}

int vfs_mount(struct inode *mount_point, dev_t block_dev, const char *filesystem, const char *devname, const char *mountname)
{
    struct super_block *super;
    int ret;

    if (flag_test(&mount_point->flags, INO_MOUNT))
        return -EBUSY;

    kp(KP_TRACE, "super_get...\n");
    super = super_get(block_dev, filesystem);
    if (!super)
        return -EINVAL;

    kp(KP_TRACE, "super_mount...\n");
    ret = super_mount(super, mount_point);
    if (ret) {
        kp(KP_TRACE, "super->root: %p\n", super->root);
        inode_put(super->root);
        super_put(super);
    }
    kp(KP_TRACE, "mount done!\n");

    mount_list_add(super->dev, devname, mountname, filesystem);

    return ret;
}

static int super_umount(struct super_block *super)
{
    int ret;
    dev_t dev = super->dev;

    ret = inode_clear_super(super);
    if (ret)
        return ret;

    ret = super_put(super);
    if (ret)
        return ret;

    mount_list_del(dev);

    return 0;
}

static int mount_seq_start(struct seq_file *seq)
{
    mutex_lock(&mount_list_lock);
    return seq_list_start(seq, &mount_list);
}

static void mount_seq_end(struct seq_file *seq)
{
    mutex_unlock(&mount_list_lock);
}

static int mount_seq_render(struct seq_file *seq)
{
    struct vfs_mount *mount = seq_list_get_entry(seq, struct vfs_mount, mount_point_node);

    if (mount->devname)
        return seq_printf(seq, "%s\t%s\t%s\n", mount->devname, mount->mountname, mount->filesystem);
    else
        return seq_printf(seq, "(%d,%d)\t%s\t%s\n", DEV_MAJOR(mount->dev), DEV_MINOR(mount->dev), mount->mountname, mount->filesystem);
}

static int mount_seq_next(struct seq_file *seq)
{
    return seq_list_next(seq, &mount_list);
}

const static struct seq_file_ops mount_seq_file_ops = {
    .start = mount_seq_start,
    .end = mount_seq_end,
    .render = mount_seq_render,
    .next = mount_seq_next,
};

static int mount_file_seq_open(struct inode *inode, struct file *filp)
{
    return seq_open(filp, &mount_seq_file_ops);
}

const struct file_ops mount_file_ops = {
    .open = mount_file_seq_open,
    .lseek = seq_lseek,
    .read = seq_read,
    .release = seq_release,
};

int vfs_umount(struct super_block *sb)
{
    return super_umount(sb);
}
