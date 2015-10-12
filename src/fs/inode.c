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
#include <mm/kmalloc.h>
#include <arch/task.h>

#include <fs/block.h>
#include <fs/super.h>
#include <fs/file.h>
#include <fs/inode.h>

#define INODE_HASH_SIZE 512

static struct {
    spinlock_t lock;

    struct hlist_head inode_hashes[INODE_HASH_SIZE];

    ino_t next_ino;
} inode_list = {
    .lock = SPINLOCK_INIT("Inode table lock"),
    .inode_hashes = { { .first = NULL } },
    .next_ino = 1,
};

#define INODE_LIST_GROW_COUNT 20

static inline int inode_hash_get(dev_t dev, ino_t ino)
{
    return (ino ^ (DEV_MAJOR(dev) + DEV_MINOR(dev))) % INODE_HASH_SIZE;
}

static void inode_hash(struct inode *inode)
{
    int hash = inode_hash_get(inode->dev, inode->ino);
    if (hlist_hashed(&inode->hash_entry))
        hlist_del(&inode->hash_entry);

    using_spinlock(&inode_list.lock)
        hlist_add(inode_list.inode_hashes + hash, &inode->hash_entry);
}

void inode_put(struct inode *inode)
{
    struct super_block *sb = inode->sb;
    atomic_dec(&inode->ref);

    using_inode_lock(inode)
        if (inode->dirty && atomic_get(&inode->ref) == 0)
            sb->ops->inode_write(sb, inode);
}

struct inode *inode_dup(struct inode *i)
{
    atomic_inc(&i->ref);

    return i;
}

struct inode *inode_get(struct super_block *sb, ino_t ino)
{
    int hash = inode_hash_get(sb->dev, ino);
    struct inode *inode = NULL;

    using_spinlock(&inode_list.lock)
        hlist_foreach_entry(&inode_list.inode_hashes[hash], inode, hash_entry)
            if (inode->ino == ino && inode->dev == sb->dev)
                break;

    if (inode)
        goto return_inode;

    inode = sb_inode_read(sb, ino);

    if (!inode)
        return NULL;

    inode_hash(inode);

  return_inode:
    atomic_inc(&inode->ref);
    return inode;
}

int fd_open(struct inode *inode, struct file **filp)
{
    int fd = fd_get_free();
    int ret;

    if (fd == -1)
        return -ENFILE;

    if (!inode->ops || !inode->ops->file_open)
        return -ENOTSUP;

    ret = (inode->ops->file_open) (inode, filp);

    if (ret)
        return ret;

    file_fd_add(fd, *filp);

    return fd;
}

int fd_close(int fd)
{
    int ret;
    struct file *filp;

    if (fd >= NOFILE)
        return -EMFILE;


    ret = file_fd_get(fd, &filp);

    if (ret)
        return ret;

    inode_file_close(filp->inode, filp);

    file_fd_del(fd);

    return 0;
}

