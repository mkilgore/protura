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

void super_sync(struct super_block *sb)
{
    struct inode *inode;

    using_mutex(&sb->dirty_inodes_lock) {
        list_foreach_take_entry(&sb->dirty_inodes, inode, sb_dirty_entry) {
            kp(KP_TRACE, "took entry: "PRinode"\n", Pinode(inode));
            sb->ops->inode_write(sb, inode);
        }

        sb->ops->sb_write(sb);
    }
}

void sys_sync(void)
{
    kp(KP_NORMAL, "sync()\n");
    super_sync(sb_root);
}

