/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/dump_mem.h>
#include <protura/string.h>
#include <protura/initcall.h>
#include <protura/snprintf.h>
#include <protura/scheduler.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/palloc.h>
#include <protura/mm/user_check.h>
#include <protura/wait.h>
#include <protura/ida.h>
#include <protura/kparam.h>

#include <arch/spinlock.h>
#include <arch/idt.h>
#include <arch/asm.h>
#include <protura/fs/inode.h>
#include <protura/fs/super.h>
#include <protura/block/disk.h>
#include <protura/block/bcache.h>
#include <protura/block/bdev.h>
#include <protura/drivers/loop.h>

struct loop_drive {
    int loopid;
    struct inode *inode;
    list_node_t node;

    char name[LOOP_NAME_MAX];

    /* disk==NULL after unregister */
    struct disk *disk;
};

#define LOOP_DRIVE_INIT(drive) \
    { \
        .node = LIST_NODE_INIT((drive).node), \
    }

static inline void loop_drive_init(struct loop_drive *drive)
{
    *drive = (struct loop_drive)LOOP_DRIVE_INIT(*drive);
}

static list_head_t loop_drive_list = LIST_HEAD_INIT(loop_drive_list);
static mutex_t loop_drive_list_lock = MUTEX_INIT(loop_drive_list_lock);

struct loop_block_work {
    struct work work;

    struct loop_drive *drive;
    struct block *block;
};

/* FIXME: We should have a thread per loop device */
static struct workqueue loop_block_queue = WORKQUEUE_INIT(loop_block_queue);

static void loop_block_sync_callback(struct work *work)
{
    struct loop_block_work *loop_block = container_of(work, struct loop_block_work, work);
    struct loop_drive *drive = loop_block->drive;
    struct block *b = loop_block->block;
    struct block_device *loop_bdev = b->bdev;

    int is_write = flag_test(&b->flags, BLOCK_DIRTY);

    size_t dest_block_size = drive->inode->sb->bdev->block_size;

    if (b->block_size > dest_block_size) {
        int blocks = b->block_size / dest_block_size;
        off_t location = b->real_sector * (1 << loop_bdev->disk->min_block_size_shift);
        sector_t dest_block = location / dest_block_size;

        int i;
        for (i = 0; i < blocks; i++) {
            sector_t real_dest_block = drive->inode->ops->bmap_alloc(drive->inode, dest_block + i);
            struct block *dest_b;

            dest_b = block_getlock(drive->inode->sb->bdev, real_dest_block);

            if (!is_write) {
                memcpy(b->data + i * dest_block_size, dest_b->data, dest_block_size);
                block_unlockput(dest_b);
            } else {
                memcpy(dest_b->data, b->data + i * dest_block_size, dest_block_size);
                block_mark_dirty(dest_b);
                block_submit(dest_b);
                block_wait_for_sync(dest_b);
            }
        }
    } else {
        off_t location = b->real_sector * (1 << loop_bdev->disk->min_block_size_shift);
        sector_t dest_block = location / dest_block_size;
        off_t offset = (b->real_sector * (1 << loop_bdev->disk->min_block_size_shift)) % dest_block_size;

        sector_t real_dest_block = drive->inode->ops->bmap_alloc(drive->inode, dest_block);

        struct block *dest_b = block_getlock(drive->inode->sb->bdev, real_dest_block);

        if (!is_write) {
            memcpy(b->data, dest_b->data + offset, b->block_size);
            block_unlockput(dest_b);
        } else {
            memcpy(dest_b->data + offset, b->data, b->block_size);
            block_mark_dirty(dest_b);
            block_submit(dest_b);
            block_wait_for_sync(dest_b);
        }
    }

    block_mark_synced(b);
    block_unlockput(b);

    kfree(loop_block);
}

static void loop_sync_block(struct disk *disk, struct block *b)
{
    if (flag_test(&b->flags, BLOCK_VALID) && !flag_test(&b->flags, BLOCK_DIRTY)) {
        block_unlock(b);
        return;
    }

    struct loop_drive *drive = disk->priv;
    struct loop_block_work *work = kzalloc(sizeof(*work), PAL_KERNEL);

    work->block = block_dup(b);
    work->drive = drive;

    work_init_workqueue(&work->work, loop_block_sync_callback, &loop_block_queue);
    flag_set(&work->work.flags, WORK_ONESHOT);

    work_schedule(&work->work);
}

#define LOOP_MINOR_SHIFT 8
#define LOOP_MAX_DISKS 32

static uint32_t loop_ids[LOOP_MAX_DISKS / 32];
static struct ida loop_ida = IDA_INIT(loop_ids, LOOP_MAX_DISKS);

static void loop_drive_destroy(struct loop_drive *drive)
{
    list_del(&drive->node);

    inode_put(drive->inode);
    ida_putid(&loop_ida, drive->loopid);

    kfree(drive);
}

static void loop_disk_put(struct disk *disk)
{
    struct loop_drive *drive = disk->priv;

    using_mutex(&loop_drive_list_lock)
        loop_drive_destroy(drive);
}

static struct disk_ops loop_disk_ops = {
    .sync_block = loop_sync_block,
    .put = loop_disk_put,
};

static int loop_create_disk(struct loop_drive *drive)
{
    int index = ida_getid(&loop_ida);
    if (index == -1)
        return -EINVAL; /* FIXME: Get right return code */

    drive->loopid = index;

    struct disk *disk = disk_alloc();

    drive->disk = disk;

    snprintf(disk->name, sizeof(disk->name), "loop%d", index);

    disk->ops = &loop_disk_ops;

    disk->major = BLOCK_DEV_LOOP;
    disk->first_minor = index << LOOP_MINOR_SHIFT;
    disk->minor_count = 1 << LOOP_MINOR_SHIFT;

    disk->min_block_size_shift = log2(512);

    disk->priv = drive;

    /* capacity is a count of 'real sectors' */
    disk_capacity_set(disk, drive->inode->size / 512);

    disk_register(disk);

    return index;
}

static int loop_create(struct user_buffer arg)
{
    int err;
    struct file *user_filp;
    struct loopctl_create create;
    struct loop_drive *new_drive;

    err = user_copy_to_kernel(&create, arg);
    if (err)
        return err;

    err = fd_get_checked(create.fd, &user_filp);
    if (err)
        return err;

    new_drive = kmalloc(sizeof(*new_drive), PAL_KERNEL);
    if (!new_drive)
        return -ENOMEM;

    loop_drive_init(new_drive);
    new_drive->inode = inode_dup(user_filp->inode);

    memcpy(new_drive->name, create.loop_name, LOOP_NAME_MAX);
    new_drive->name[LOOP_NAME_MAX - 1] = '\0';

    err = loop_create_disk(new_drive);
    if (err < 0) {
        inode_put(new_drive->inode);
        kfree(new_drive);
        return err;
    }

    create.loop_number = err;

    using_mutex(&loop_drive_list_lock)
        list_add_tail(&loop_drive_list, &new_drive->node);

    err = user_copy_from_kernel(arg, create);
    if (err)
        return err;

    return 0;
}

static int loop_destroy(struct user_buffer arg)
{
    struct loopctl_destroy destroy;
    struct disk *disk = NULL;

    int err = user_copy_to_kernel(&destroy, arg);
    if (err)
        return err;

    using_mutex(&loop_drive_list_lock) {
        struct loop_drive *drive;

        list_foreach_entry(&loop_drive_list, drive, node)
            if (drive->loopid == destroy.loop_number)
                break;

        if (list_ptr_is_head(&loop_drive_list, &drive->node))
            return -ENOENT;

        if (drive->disk) {
            disk = drive->disk;
            drive->disk = NULL;
        }
    }

    if (disk) {
        disk_unregister(disk);
        disk_put(disk);
    }

    return 0;
}

static int loop_status(struct user_buffer arg)
{
    struct loopctl_status status;
    int err;

    err = user_copy_to_kernel(&status, arg);
    if (err)
        return err;

    using_mutex(&loop_drive_list_lock) {
        int id = status.loop_number;
        struct loop_drive *drive;

        list_foreach_entry(&loop_drive_list, drive, node)
            if (drive->loopid == id)
                break;

        if (list_ptr_is_head(&loop_drive_list, &drive->node))
            return -ENOENT;

        memcpy(status.loop_name, drive->name, LOOP_NAME_MAX);

        err = user_copy_from_kernel(arg, status);
        if (err)
            return err;
    }

    return 0;
}

static int loop_ioctl(struct file *filp, int cmd, struct user_buffer arg)
{
    switch (cmd) {
    case LOOPCTL_CREATE:
        return loop_create(arg);

    case LOOPCTL_DESTROY:
        return loop_destroy(arg);

    case LOOPCTL_STATUS:
        return loop_status(arg);
    }

    return -ENOSYS;
}

struct file_ops loop_control_ops = {
    .ioctl = loop_ioctl,
};

static void loop_init(void)
{
    workqueue_start_multiple(&loop_block_queue, "loop", 4);
}
initcall_subsys(block_loop, loop_init);

