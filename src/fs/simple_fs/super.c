/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/list.h>
#include <protura/semaphore.h>
#include <protura/dump_mem.h>
#include <mm/kmalloc.h>

#include <arch/spinlock.h>
#include <fs/block.h>
#include <fs/file_system.h>
#include <fs/simple_fs.h>

static struct inode *simple_fs_read_inode(struct super_block *sb, kino_t ino)
{
    struct simple_fs_inode *inode;
    struct simple_fs_disk_inode *diski;
    struct block *b;

    inode = kzalloc(sizeof(*inode), PAL_KERNEL);

    mutex_init(&inode->i.lock);

    inode->i.ino = ino;
    inode->i.sb = sb;
    inode->i.dev = sb->dev;

    kprintf("Reading inode %d\n", ino);

    using_block(sb->dev, ino, b) {
        diski = (struct simple_fs_disk_inode *)b->data;

        inode->i.size = diski->size;
        memcpy(inode->contents, diski->sectors, sizeof(diski->sectors));
    }

    kprintf("Inode Size: %ld\n", inode->i.size);

    inode->i.valid = 1;

    return &inode->i;
}

static void simple_fs_write_inode(struct super_block *sb, struct inode *i)
{
    struct simple_fs_disk_inode *diski;
    struct simple_fs_inode *inode = container_of(i, struct simple_fs_inode, i);
    struct block *b;

    if (!inode->i.dirty)
        return ;

    using_block(sb->dev, inode->i.ino, b) {
        diski = (struct simple_fs_disk_inode *)b->data;

        diski->size = inode->i.size;
        memcpy(diski->sectors, inode->contents, sizeof(inode->contents));

        block_mark_dirty(b);
    }

    inode->i.dirty = 0;
}

static void simple_fs_put_inode(struct super_block *sb, struct inode *inode)
{
    simple_fs_write_inode(sb, inode);
}

static void simple_fs_delete_inode(struct super_block *sb, struct inode *inode)
{
    /* We can't really delete. This isn't really a full featured FS... */
}

static void simple_fs_write_sb(struct super_block *sb)
{
    /* Don't do this please */
}

static void simple_fs_put_sb(struct super_block *sb)
{
    struct simple_fs_super_block *s = container_of(sb, struct simple_fs_super_block, sb);
    simple_fs_write_sb(sb);

    inode_put(sb->root);
    kfree(s);
}

static struct super_block_ops simple_fs_sb_ops = {
    .read_inode = simple_fs_read_inode,
    .write_inode = simple_fs_write_inode,
    .put_inode = simple_fs_put_inode,
    .delete_inode = simple_fs_delete_inode,
    .write_sb = simple_fs_write_sb,
    .put_sb = simple_fs_put_sb,
};

struct super_block *simple_fs_read_sb(kdev_t dev)
{
    struct simple_fs_super_block *sb;
    struct simple_fs_disk_sb *disksb;
    struct block *b;

    sb = kzalloc(sizeof(*sb), PAL_KERNEL);

    kprintf("kzalloc:sb: %p\n", sb);

    sb->sb.bdev = block_dev_get(dev);
    sb->sb.dev = dev;
    sb->sb.ops = &simple_fs_sb_ops;

    using_block(dev, 0, b) {
        disksb = (struct simple_fs_disk_sb *)b->data;

        sb->file_count = disksb->file_count;
        sb->root_ino = disksb->root_ino;
    }

    kprintf("Read block 0, files: %d, root_ino: %d\n", sb->file_count, sb->root_ino);

    sb->sb.root = inode_get(&sb->sb, sb->root_ino);

    return &sb->sb;
}

static struct file_system simple_fs_fs = {
    .name = "simple_fs",
    .read_sb = simple_fs_read_sb,
};

void simple_fs_init(void)
{
    file_system_register(&simple_fs_fs);
}

