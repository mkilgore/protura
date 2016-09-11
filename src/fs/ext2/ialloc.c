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
#include <protura/dump_mem.h>
#include <protura/mm/kmalloc.h>

#include <arch/spinlock.h>
#include <protura/fs/block.h>
#include <protura/fs/char.h>
#include <protura/fs/stat.h>
#include <protura/fs/file.h>
#include <protura/fs/inode_table.h>
#include <protura/fs/file_system.h>
#include <protura/fs/vfs.h>
#include <protura/fs/ext2.h>
#include "ext2_internal.h"

static ino_t __ext2_check_block_group(struct ext2_super_block *sb, int group_no)
{
    struct block *b;
    struct ext2_disk_block_group *group;
    int loc;
    ino_t ino;
    int inode_start = 0;

    group = sb->groups + group_no;

    if (group->inode_unused_total == 0)
        return 0;

    /* Group 0 includes the reserved inodes */
    if (group_no == 0)
        inode_start = sb->disksb.first_inode;

    using_block(sb->sb.dev, group->block_nr_inode_bitmap, b) {
        loc = bit_find_next_zero(b->data, sb->block_size, inode_start);

        ino = loc + group_no * sb->block_size * CHAR_BIT + 1;

        kp_ext2(sb, "inode bitmap: %d\n", group->block_nr_inode_bitmap);
        kp_ext2(sb, "Free inode: %d, loc: %d\n", ino, loc);

        bit_set(b->data, loc);
        group->inode_unused_total--;

        block_mark_dirty(b);
    }

    return ino;
}

int ext2_inode_new(struct super_block *sb, struct inode **result)
{
    struct ext2_super_block *ext2sb = container_of(sb, struct ext2_super_block, sb);
    struct ext2_inode *ext2_inode;
    int i, ret = 0;
    ino_t ino = 0;
    struct inode *inode;
    sector_t inode_group_blk = 0;
    int inode_group_blk_offset = 0;

    inode = (sb->ops->inode_alloc) (sb);
    kp_ext2(sb, "ialloc: inode_alloc: %p\n", inode);

    if (!inode)
        return -ENOSPC;

    ext2_inode = container_of(inode, struct ext2_inode, i);

    using_super_block(sb) {
        for (i = 0; i < ext2sb->block_group_count && !ino; i++)
            if ((ino = __ext2_check_block_group(ext2sb, i)) != 0)
                break;

        if (ino) {
            int entry;

            entry = (ino - 1) % ext2sb->disksb.inodes_per_block_group;
            kp_ext2(sb, "ialloc: entry: %d\n", entry);

            inode_group_blk = ext2sb->groups[i].block_nr_inode_table;
            kp_ext2(sb, "ialloc: group start: %d\n", inode_group_blk);

            inode_group_blk += (entry * sizeof(struct ext2_disk_inode)) / ext2sb->block_size;
            kp_ext2(sb, "ialloc: inode group block: %d\n", inode_group_blk);

            inode_group_blk_offset = entry % (ext2sb->block_size / sizeof(struct ext2_disk_inode));
            kp_ext2(sb, "ialloc: inode group block offset: %d\n", inode_group_blk_offset);
        }

        kp_ext2(sb, "ialloc: Found new inode: %d\n", ino);
        kp_ext2(sb, "ialloc: group: %d\n", i);

        if (ino)
            ext2sb->disksb.inode_unused_total--;
        else
            ret = -ENOSPC;
    }

    if (ret)
        goto cleanup_inode;

    ext2_inode->i.sb = sb;
    ext2_inode->i.sb_dev = sb->dev;
    ext2_inode->i.ino = ino;
    ext2_inode->inode_group_blk_nr = inode_group_blk;
    ext2_inode->inode_group_blk_offset = inode_group_blk_offset;

    atomic_inc(&ext2_inode->i.ref);

    inode_add(&ext2_inode->i);

    *result = &ext2_inode->i;
    return 0;

  cleanup_inode:
    (sb->ops->inode_dealloc) (sb, inode);
    return ret;
}

