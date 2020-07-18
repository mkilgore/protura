/*
 * Copyright (C) 2020 Matt Kilgore
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
#include <protura/fs/procfs.h>
#include <protura/fs/vfs.h>
#include <protura/fs/fs.h>

enum {
    /* This indicates an umount is being attempted or taking place.
     * Wait on flags_queue and then check for the mount again */
    VFS_MOUNT_UNMOUNTING,
};

struct vfs_mount {
    list_node_t mount_point_node;

    struct super_block *sb;
    struct inode *root;
    struct inode *covered;

    flags_t flags;

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

static spinlock_t super_lock = SPINLOCK_INIT();
static list_head_t super_block_list = LIST_HEAD_INIT(super_block_list);
static list_head_t mount_list = LIST_HEAD_INIT(mount_list);
static struct wait_queue umount_queue = WAIT_QUEUE_INIT(umount_queue);

static struct super_block *super_alloc(struct file_system *fs)
{
    if (fs->super_alloc)
        return fs->super_alloc();

    struct super_block *sb = kzalloc(sizeof(*sb), PAL_KERNEL);
    super_block_init(sb);

    return sb;
}

static void super_unused_dealloc(struct super_block *sb)
{
    if (!sb->bdev)
        block_dev_anon_put(sb->dev);

    if (sb->fs->super_dealloc)
        (sb->fs->super_dealloc) (sb);
    else
        kfree(sb);
}

static void __super_put(struct super_block *sb)
{
    if (!--sb->count)
        super_unused_dealloc(sb);
}

static void super_put(struct super_block *sb)
{
    using_spinlock(&super_lock)
        __super_put(sb);
}

static void __super_sync(struct super_block *sb)
{
    if (sb->ops->sb_write)
        sb->ops->sb_write(sb);
}

static void vfs_mount_dealloc(struct vfs_mount *vfsmount)
{
    kfree(vfsmount->devname);
    kfree(vfsmount->mountname);
    kfree(vfsmount->filesystem);

    kfree(vfsmount);
}

/* struct vfs_mount should be marked VFS_MOUNT_UNMOUNTING, preventing anybody
 * else from trying to use it outside the super_lock */
static int vfs_mount_try_umount(struct vfs_mount *vfsmount)
{
    struct super_block *sb = vfsmount->sb;

    mutex_lock(&sb->umount_lock);

    int ret = inode_clear_super(sb, vfsmount->root);
    if (ret) {
        mutex_unlock(&sb->umount_lock);
        /* It didn't work, drop the flag and signal any waiters */
        using_spinlock(&super_lock)
            flag_clear(&vfsmount->flags, VFS_MOUNT_UNMOUNTING);

        wait_queue_wake(&umount_queue);
        return ret;
    }


    if (sb->ops && sb->ops->sb_put)
        (sb->ops->sb_put) (sb);
    else
        __super_sync(sb);

    flag_set(&sb->flags, SUPER_IS_DEAD);

    using_spinlock(&super_lock) {
        list_del(&vfsmount->mount_point_node);
        list_del(&sb->list_entry);
    }

    mutex_unlock(&sb->umount_lock);

    if (sb->bdev)
        block_dev_sync(sb->bdev, sb->dev, 1);

    super_put(sb);
    inode_put(vfsmount->covered);
    wait_queue_wake(&umount_queue);

    vfs_mount_dealloc(vfsmount);

    return 0;
}

/* FS's with no backing device can be mounted any number of times */
static struct super_block *super_get_nodev(struct file_system *fs)
{
    struct super_block *sb = super_alloc(fs);

    sb->dev = block_dev_anon_get();
    sb->count++;
    sb->fs = fs;
    return sb;
}

static struct super_block *super_get_or_create(dev_t device, struct file_system *fs)
{
    struct super_block *sb, *tmp = NULL;

    if (device == 0)
        return super_get_nodev(fs);

  again:
    spinlock_acquire(&super_lock);
    list_foreach_entry(&super_block_list, sb, list_entry) {
        if (sb->dev == device) {
            sb->count++;

            spinlock_release(&super_lock);

            if (tmp)
                super_unused_dealloc(tmp);

            return sb;
        }
    }

    if (tmp) {
        list_add_tail(&super_block_list, &tmp->list_entry);
        tmp->count++;

        spinlock_release(&super_lock);

        return tmp;
    }
    spinlock_release(&super_lock);

    struct block_device *bdev = block_dev_get(device);
    if (!bdev)
        return NULL;

    tmp = super_alloc(fs);
    tmp->dev = device;
    tmp->bdev = bdev;
    tmp->fs = fs;
    goto again;
}

static void __sync_single_super(struct super_block *sb)
{
    inode_sync(sb, 1);

    __super_sync(sb);
}

void sync_all_supers(void)
{
    struct super_block *sb, *prev = NULL;

    spinlock_acquire(&super_lock);
    list_foreach_entry(&super_block_list, sb, list_entry) {
        sb->count++;

        spinlock_release(&super_lock);

        using_mutex(&sb->umount_lock) {
            if (flag_test(&sb->flags, SUPER_IS_VALID) && !flag_test(&sb->flags, SUPER_IS_DEAD))
                __sync_single_super(sb);
        }

        spinlock_acquire(&super_lock);

        if (prev)
            __super_put(prev);

        prev = sb;
    }

    if (prev)
        __super_put(prev);

    spinlock_release(&super_lock);
}

int vfs_mount(struct inode *mount_point, dev_t block_dev, const char *filesystem, const char *devname, const char *mountname)
{
    int ret = 0;
    struct file_system *fs = file_system_lookup(filesystem);
    if (!fs)
        return -EINVAL;

    if (flag_test(&fs->flags, FILE_SYSTEM_NODEV) && block_dev)
        return -EINVAL;

    if (!flag_test(&fs->flags, FILE_SYSTEM_NODEV) && !block_dev)
        return -EINVAL;

    struct super_block *sb = super_get_or_create(block_dev, fs);
    if (!sb)
        return -EINVAL;

    mutex_lock(&sb->umount_lock);

    if (flag_test(&sb->flags, SUPER_IS_VALID)) {
        ret = -EBUSY;
        goto unlock_sb;
    }

    ret = (fs->read_sb2) (sb);
    if (ret)
        goto unlock_sb;

    flag_set(&sb->flags, SUPER_IS_VALID);
    mutex_unlock(&sb->umount_lock);

    struct inode *root = inode_get(sb, sb->root_ino);
    if (!root) {
        ret = -EBUSY;
        goto put_sb;
    }

    struct vfs_mount *vfsmount = kmalloc(sizeof(*vfsmount), PAL_KERNEL);
    vfs_mount_init(vfsmount);

    vfsmount->dev = block_dev;
    vfsmount->mountname = kstrdup(mountname, PAL_KERNEL);
    vfsmount->filesystem = kstrdup(filesystem, PAL_KERNEL);

    if (devname)
        vfsmount->devname = kstrdup(devname, PAL_KERNEL);

    vfsmount->root = root;
    vfsmount->sb = sb;

    /* Special case for root - it has no covered inode */
    if (mount_point)
        vfsmount->covered = inode_dup(mount_point);

    /* Make sure the mount_point inode doesn't already have a mount point
     * associated with it */
    using_spinlock(&super_lock) {
        struct vfs_mount *tmp;

        list_foreach_entry(&mount_list, tmp, mount_point_node) {
            if (tmp->covered == mount_point) {
                ret = -EBUSY;
                break;
            }
        }

        if (!ret)
            list_add_tail(&mount_list, &vfsmount->mount_point_node);
    }

    if (ret) {
        /* We have to undo everything we did before trying to add the mount */
        inode_put(vfsmount->covered);

        inode_clear_super(vfsmount->sb, vfsmount->root);
        super_put(vfsmount->sb);

        vfs_mount_dealloc(vfsmount);
    }

    return ret;

  unlock_sb:
    mutex_unlock(&sb->umount_lock);

  put_sb:
    super_put(sb);
    return ret;
}

/* Drops and reacquire's the super_lock */
static void wait_for_umount(void)
{
    struct task *current = cpu_get_local()->current;

    scheduler_set_sleeping();
    wait_queue_register(&umount_queue, &current->wait);

    spinlock_release(&super_lock);

    scheduler_task_yield();

    spinlock_acquire(&super_lock);
    wait_queue_unregister(&current->wait);
    scheduler_set_running();
}

struct inode *vfs_get_mount(struct inode *mount_point)
{
    struct vfs_mount *vfsmount;

    using_spinlock(&super_lock) {
  again:
        list_foreach_entry(&mount_list, vfsmount, mount_point_node) {
            if (vfsmount->covered == mount_point) {

                /* If we found a mount point, but a umount is being attempted,
                 * wait on the queue and then try again, it might be gone */
                if (!flag_test(&vfsmount->flags, VFS_MOUNT_UNMOUNTING)) {
                    return inode_dup(vfsmount->root);
                } else {
                    wait_for_umount();
                    goto again;
                }
            }
        }
    }

    return NULL;
}

int vfs_umount(struct super_block *sb)
{
    struct vfs_mount *vfsmount, *found = NULL;

    using_spinlock(&super_lock) {
        list_foreach_entry(&mount_list, vfsmount, mount_point_node) {
            if (vfsmount->sb == sb) {
                if (flag_test(&vfsmount->flags, VFS_MOUNT_UNMOUNTING))
                    return -EBUSY;

                flag_set(&vfsmount->flags, VFS_MOUNT_UNMOUNTING);
                found = vfsmount;
                break;
            }
        }
    }

    if (!found)
        return -EINVAL;

    /* This either completely umounts the mountpoint, or drops the VFS_MOUNT_UNMOUNTING flag */
    return vfs_mount_try_umount(found);
}

int mount_root(dev_t device, const char *fsystem)
{
    struct block_device *bdev = block_dev_get(device);
    if (!bdev) {
        panic("BLOCK DEVICE %d:%d does not exist!\n", DEV_MAJOR(device), DEV_MINOR(device));
    }

    int ret = vfs_mount(NULL, device, fsystem, NULL, "/");
    if (ret) {
        panic("UNABLE TO MOUNT ROOT DEVICE!!! Err: %d\n", ret);
        return ret;
    }

    using_spinlock(&super_lock) {
        struct vfs_mount *root_mount = list_first_entry(&mount_list, struct vfs_mount, mount_point_node);
        ino_root = inode_dup(root_mount->root);
    }

    return 0;
}

static int mount_seq_start(struct seq_file *seq)
{
    spinlock_acquire(&super_lock);
    return seq_list_start(seq, &mount_list);
}

static void mount_seq_end(struct seq_file *seq)
{
    spinlock_release(&super_lock);
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
