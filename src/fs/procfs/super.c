/*
 * Copyright (C) 2016 Matt Kilgore
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
#include <protura/mm/kmalloc.h>
#include <protura/mm/vm.h>
#include <arch/idt.h>
#include <protura/scheduler.h>

#include <arch/spinlock.h>
#include <protura/fs/block.h>
#include <protura/fs/char.h>
#include <protura/fs/stat.h>
#include <protura/fs/file.h>
#include <protura/fs/inode_table.h>
#include <protura/fs/file_system.h>
#include <protura/fs/vfs.h>
#include <protura/fs/procfs.h>
#include "procfs_internal.h"

static struct inode *procfs_inode_alloc(struct super_block *sb)
{
    struct procfs_inode *inode = kzalloc(sizeof(*inode), PAL_KERNEL);

    inode_init(&inode->i);

    return &inode->i;
}

static int procfs_inode_dealloc(struct super_block *sb, struct inode *inode)
{
    struct procfs_inode *i = container_of(inode, struct procfs_inode, i);
    kfree(i);
    return 0;
}

static int procfs_inode_read(struct super_block *sb, struct inode *inode)
{
    struct procfs_inode *pinode = container_of(inode, struct procfs_inode, i);
    struct procfs_node *node;

    node = procfs_hash_get_node(pinode->i.ino);
    if (!node)
        return -ENOENT;

    pinode->i.sb_dev = sb->dev;
    pinode->i.dev_no = 0;
    pinode->i.mode = node->mode;
    atomic32_set(&pinode->i.nlinks, node->nlinks);

    pinode->i.blocks = 0;
    pinode->i.block_size = PG_SIZE;
    pinode->i.size = 0;

    pinode->i.sb = sb;
    pinode->node = node;

    if (S_ISDIR(pinode->i.mode)) {
        pinode->i.ops = &procfs_dir_inode_ops;
        pinode->i.default_fops = &procfs_dir_file_ops;
    } else if (S_ISREG(inode->mode)) {
        pinode->i.ops = &procfs_file_inode_ops;
        pinode->i.default_fops = &procfs_file_file_ops;
    }

    inode_set_valid(&pinode->i);

    return 0;
}

static int procfs_sb_put(struct super_block *sb)
{
    block_dev_anon_put(sb->dev);
    kfree(sb);

    return 0;
}

static struct super_block_ops procfs_sb_ops = {
    .inode_alloc = procfs_inode_alloc,
    .inode_dealloc = procfs_inode_dealloc,
    .inode_read = procfs_inode_read,
    .inode_write = NULL, /* Read-only */
    .inode_delete = NULL,
    .sb_write = NULL,
    .sb_put = procfs_sb_put,
};

static struct super_block *procfs_read_sb(dev_t dev)
{
    struct super_block *sb;

    sb = kzalloc(sizeof(*sb), PAL_KERNEL);
    super_block_init(sb);

    sb->dev = block_dev_anon_get();
    sb->ops = &procfs_sb_ops;
    sb->bdev = NULL;

    sb->root = inode_get(sb, PROCFS_ROOT_INO);

    return sb;
}

static struct file_system procfs_fs = {
    .name = "proc",
    .read_sb = procfs_read_sb,
    .fs_list_entry = LIST_NODE_INIT(procfs_fs.fs_list_entry),
};

void procfs_init(void)
{
    file_system_register(&procfs_fs);

    procfs_hash_add_node(&procfs_root.node);

#if 0
    struct procfs_dir *dir1, *dir2;
    dir1 = procfs_register_dir(&procfs_root, "new_dir1");
    dir2 = procfs_register_dir(&procfs_root, "new_dir2");

    procfs_register_entry(&procfs_root, "entry1", NULL);
    procfs_register_entry(&procfs_root, "entry2", NULL);
    procfs_register_entry(&procfs_root, "entry3", NULL);
    procfs_register_entry(&procfs_root, "entry4", NULL);
    procfs_register_entry(&procfs_root, "entry5", NULL);
    procfs_register_entry(&procfs_root, "entry6", NULL);

    procfs_register_entry(dir1, "entry1", NULL);
    procfs_register_entry(dir1, "entry2", NULL);
    procfs_register_entry(dir1, "entry3", NULL);
    procfs_register_entry(dir1, "entry4", NULL);
    procfs_register_entry(dir1, "entry5", NULL);
    procfs_register_entry(dir1, "entry6", NULL);

    procfs_register_entry(dir2, "entry1", NULL);
    procfs_register_entry(dir2, "entry2", NULL);
    procfs_register_entry(dir2, "entry3", NULL);
    procfs_register_entry(dir2, "entry4", NULL);
    procfs_register_entry(dir2, "entry5", NULL);
    procfs_register_entry(dir2, "entry6", NULL);
#endif

    procfs_register_entry(&procfs_root, "interrupts", interrupt_stats_read);
    procfs_register_entry(&procfs_root, "tasks", scheduler_tasks_read);
    procfs_register_entry(&procfs_root, "filesystems", file_systeam_list_read);
}

