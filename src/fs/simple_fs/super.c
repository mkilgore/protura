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
#include <fs/file.h>
#include <fs/file_system.h>
#include <fs/simple_fs.h>

static sector_t simple_fs_get_sector(struct simple_fs_super_block *sb, ino_t ino)
{
    struct block *b;
    sector_t ret = 0;

    using_block(sb->sb.dev, sb->inode_map, b) {
        struct simple_fs_disk_inode_map *map = (struct simple_fs_disk_inode_map *)b->data;
        int i;

        for (i = 0; i < sb->inode_count; i++) {
            if (map[i].ino == ino) {
                ret = map[i].sector;
                break;
            }
        }
    }

    return ret;
}

static struct inode *simple_fs_inode_read(struct super_block *super, ino_t ino)
{
    struct simple_fs_super_block *sb = container_of(super, struct simple_fs_super_block, sb);
    struct simple_fs_inode *inode;
    struct simple_fs_disk_inode *diski;
    struct block *b;
    sector_t ino_sector;

    inode = kzalloc(sizeof(*inode), PAL_KERNEL);

    mutex_init(&inode->i.lock);

    inode->i.ino = ino;
    inode->i.sb = super;
    inode->i.dev = super->dev;
    inode->i.ops = &simple_fs_inode_ops;
    inode->i.default_file_ops = &simple_fs_file_ops;

    ino_sector = simple_fs_get_sector(sb, ino);

    using_block(super->dev, ino_sector, b) {
        diski = (struct simple_fs_disk_inode *)b->data;

        inode->i.size = diski->size;
        inode->i.mode = diski->mode;
        memcpy(inode->contents, diski->sectors, sizeof(diski->sectors));
    }

    inode->i.valid = 1;

    return &inode->i;
}

static int simple_fs_inode_write(struct super_block *super, struct inode *i)
{
    struct simple_fs_super_block *sb = container_of(super, struct simple_fs_super_block, sb);
    struct simple_fs_disk_inode *diski;
    struct simple_fs_inode *inode = container_of(i, struct simple_fs_inode, i);
    struct block *b;
    sector_t ino_sector;

    if (!inode->i.dirty)
        return 0;

    ino_sector = simple_fs_get_sector(sb, inode->i.ino);

    using_block(super->dev, ino_sector, b) {
        diski = (struct simple_fs_disk_inode *)b->data;

        diski->size = inode->i.size;
        diski->mode = inode->i.mode;
        memcpy(diski->sectors, inode->contents, sizeof(inode->contents));

        block_mark_dirty(b);
    }

    inode->i.dirty = 0;

    return 0;
}

static int simple_fs_inode_put(struct super_block *sb, struct inode *inode)
{
    simple_fs_inode_write(sb, inode);
    kfree(inode);

    return 0;
}

static int simple_fs_inode_delete(struct super_block *sb, struct inode *inode)
{
    /* We can't really delete. This isn't really a full featured FS... */
    return -ENOTSUP;
}

static int simple_fs_sb_write(struct super_block *sb)
{
    /* Don't do this please */
    return -ENOTSUP;
}

static int simple_fs_sb_put(struct super_block *sb)
{
    struct simple_fs_super_block *s = container_of(sb, struct simple_fs_super_block, sb);
    simple_fs_sb_write(sb);

    inode_put(sb->root);
    kfree(s);

    return 0;
}

static struct super_block_ops simple_fs_sb_ops = {
    .inode_read = simple_fs_inode_read,
    .inode_write = simple_fs_inode_write,
    .inode_put = simple_fs_inode_put,
    .inode_delete = simple_fs_inode_delete,
    .sb_write = simple_fs_sb_write,
    .sb_put = simple_fs_sb_put,
};

struct super_block *simple_fs_read_sb(dev_t dev)
{
    struct simple_fs_super_block *sb;
    struct simple_fs_disk_sb *disksb;
    struct block *b;

    sb = kzalloc(sizeof(*sb), PAL_KERNEL);

    sb->sb.bdev = block_dev_get(dev);
    sb->sb.dev = dev;
    sb->sb.ops = &simple_fs_sb_ops;

    using_block(dev, 0, b) {
        disksb = (struct simple_fs_disk_sb *)b->data;

        sb->inode_count = disksb->inode_count;
        sb->root_ino = disksb->root_ino;
        sb->inode_map = disksb->inode_map_sector;
    }

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

