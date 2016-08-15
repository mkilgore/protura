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
#include <protura/mutex.h>
#include <protura/atomic.h>
#include <protura/mm/kmalloc.h>
#include <arch/task.h>

#include <protura/fs/block.h>
#include <protura/fs/super.h>
#include <protura/fs/file.h>
#include <protura/fs/stat.h>
#include <protura/fs/inode.h>
#include <protura/fs/inode_table.h>
#include <protura/fs/vfs.h>
#include <protura/fs/fs.h>

static mutex_t super_lock = MUTEX_INIT(super_lock, "super-block-list-lock");
static list_head_t super_block_list = LIST_HEAD_INIT(super_block_list);

static struct super_block *super_get(dev_t device, const char *filesystem)
{
    struct file_system *system;
    struct super_block *sb;

    system = file_system_lookup(filesystem);
    if (!system)
        return NULL;

    if (!system->read_sb)
        return NULL;

    sb = (system->read_sb) (device);

    using_mutex(&super_lock)
        list_add(&super_block_list, &sb->list_entry);

    return sb;
}

static int super_put(struct super_block *super)
{
    kp(KP_TRACE, "Super put\n");
    using_mutex(&super_lock)
        list_del(&super->list_entry);

    using_mutex(&super->super_block_lock);
    sb_put(super);

    return 0;
}

void __super_sync(struct super_block *sb)
{
    struct inode *inode;

    list_foreach_take_entry(&sb->dirty_inodes, inode, sb_dirty_entry) {
        kp(KP_TRACE, "took entry: "PRinode"\n", Pinode(inode));
        sb->ops->inode_write(sb, inode);
    }

    sb->ops->sb_write(sb);
}

void super_sync(struct super_block *sb)
{
    using_super_block(sb)
        __super_sync(sb);
}

void sys_sync(void)
{
    struct super_block *sb;
    kp(KP_NORMAL, "sync()\n");

    using_mutex(&super_lock)
        list_foreach_entry(&super_block_list, sb, list_entry)
            super_sync(sb);
}

int mount_root(dev_t device, const char *fsystem)
{
    struct super_block *sb = super_get(device, fsystem);
    if (!sb)
        return -EINVAL;

    ino_root = sb->root;

    return 0;
}

static int super_mount(struct super_block *super, struct inode *mount_point)
{
    int ret = 0;

    using_inode_mount(mount_point) {
        if (!flag_test(&mount_point->flags, INO_MOUNT)) {
            super->covered = inode_dup(mount_point);
            mount_point->mount = inode_dup(super->root);
            flag_set(&mount_point->flags, INO_MOUNT);
        } else {
            ret = -EBUSY;
        }
    }

    return ret;
}

int vfs_mount(struct inode *mount_point, dev_t block_dev, const char *filesystem)
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

    return ret;
}

static int super_umount(struct super_block *super)
{
    int ret;

    ret = inode_clear_super(super);
    if (ret)
        return ret;

    ret = super_put(super);
    if (ret)
        return ret;

    return 0;
}

int vfs_umount(struct super_block *sb)
{
    return super_umount(sb);
}

