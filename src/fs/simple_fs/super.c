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
#include <protura/mutex.h>
#include <protura/dump_mem.h>
#include <protura/mm/kmalloc.h>

#include <arch/spinlock.h>
#include <protura/fs/block.h>
#include <protura/fs/char.h>
#include <protura/fs/stat.h>
#include <protura/fs/file.h>
#include <protura/fs/inode_table.h>
#include <protura/fs/file_system.h>
#include <protura/fs/simple_fs.h>

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
    struct simple_fs_inode *s_inode;
    struct inode *inode;
    struct simple_fs_disk_inode diski;
    struct block *b;
    sector_t ino_sector;

    ino_sector = simple_fs_get_sector(sb, ino);

    using_block(super->dev, ino_sector, b)
        memcpy(&diski, b->data, sizeof(diski));

    s_inode = kzalloc(sizeof(*s_inode), PAL_KERNEL);


    inode = &s_inode->i;
    inode_init(inode);

    inode->dev = super->dev;
    inode->ops = &simple_fs_inode_ops;

    memcpy(s_inode->contents, diski.sectors, sizeof(diski.sectors));

    switch (diski.mode & S_IFMT) {
    case S_IFBLK:
        inode->dev = DEV_MAKE(diski.major, diski.minor);
        inode->bdev = block_dev_get(inode->dev);
        inode->default_fops = inode->bdev->fops;
        break;

    case S_IFCHR:
        kp(KP_TRACE, "Creating char device inode\n");
        inode->dev = DEV_MAKE(diski.major, diski.minor);
        inode->cdev = char_dev_get(inode->dev);
        inode->default_fops = inode->cdev->fops;
        break;

    default:
        inode->default_fops = &simple_fs_file_ops;
        break;
    }

    inode->ino = ino;
    inode->sb = super;

    inode->size = diski.size;
    inode->mode = diski.mode;

    inode_set_valid(inode);

    return inode;
}

static int simple_fs_inode_write(struct super_block *super, struct inode *i)
{
    struct simple_fs_super_block *sb = container_of(super, struct simple_fs_super_block, sb);
    struct simple_fs_disk_inode *diski;
    struct simple_fs_inode *inode = container_of(i, struct simple_fs_inode, i);
    struct block *b;
    sector_t ino_sector;

    if (!inode_is_dirty(i))
        return 0;

    using_inode_lock_read(i) {
        ino_sector = simple_fs_get_sector(sb, inode->i.ino);

        using_block(super->dev, ino_sector, b) {
            diski = (struct simple_fs_disk_inode *)b->data;

            diski->size = inode->i.size;
            diski->mode = inode->i.mode;
            memcpy(diski->sectors, inode->contents, sizeof(inode->contents));

            block_mark_dirty(b);
        }
    }

    inode_clear_dirty(i);

    return 0;
}

static int simple_fs_inode_dealloc(struct super_block *sb, struct inode *inode)
{
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
    .inode_dealloc = simple_fs_inode_dealloc,
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

    block_dev_set_block_size(dev, SIMPLE_FS_BLOCK_SIZE);

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
    .fs_list_entry = LIST_NODE_INIT(simple_fs_fs.fs_list_entry),
};

void simple_fs_init(void)
{
    file_system_register(&simple_fs_fs);
}

