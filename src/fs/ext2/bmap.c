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
#include <protura/fs/ext2.h>
#include "ext2_internal.h"

sector_t ext2_bmap(struct inode *i, sector_t inode_sector)
{
    const int ptrs_per_blk = i->sb->bdev->block_size / sizeof(uint32_t);
    struct ext2_inode *inode = container_of(i, struct ext2_inode, i);
    struct super_block *sb = i->sb;
    sector_t ret = SECTOR_INVALID;

    if (inode_sector < ARRAY_SIZE(inode->blk_ptrs_direct)) {
        ret = inode->blk_ptrs_direct[inode_sector];
        goto return_sector;
    }

    inode_sector -= ARRAY_SIZE(inode->blk_ptrs_direct);

    if (inode_sector < ARRAY_SIZE(inode->blk_ptrs_single) * ptrs_per_blk) {
        int single_blk = inode_sector / ptrs_per_blk;
        int direct_blk = inode_sector % ptrs_per_blk;
        struct block *b;

        using_block(sb->dev, inode->blk_ptrs_single[single_blk], b)
            ret = ((uint32_t *)b->data)[direct_blk];

        goto return_sector;
    }

    inode_sector -= ARRAY_SIZE(inode->blk_ptrs_single) * ptrs_per_blk;

    if (inode_sector < ARRAY_SIZE(inode->blk_ptrs_double) * ptrs_per_blk * ptrs_per_blk) {
        int double_blk = inode_sector / ptrs_per_blk / ptrs_per_blk;
        int single_blk = (inode_sector / ptrs_per_blk) % ptrs_per_blk;;
        int direct_blk = inode_sector % ptrs_per_blk;
        struct block *b;

        using_block(sb->dev, inode->blk_ptrs_double[double_blk], b)
            ret = ((uint32_t *)b->data)[single_blk];

        using_block(sb->dev, ret, b)
            ret = ((uint32_t *)b->data)[direct_blk];

        goto return_sector;
    }

    inode_sector -= ARRAY_SIZE(inode->blk_ptrs_double) * ptrs_per_blk * ptrs_per_blk;

    if (inode_sector < ARRAY_SIZE(inode->blk_ptrs_triple) * ptrs_per_blk * ptrs_per_blk * ptrs_per_blk) {
        int triple_blk = inode_sector / ptrs_per_blk / ptrs_per_blk / ptrs_per_blk;
        int double_blk = (inode_sector / ptrs_per_blk / ptrs_per_blk) % ptrs_per_blk;
        int single_blk = (inode_sector / ptrs_per_blk) % ptrs_per_blk;
        int direct_blk = inode_sector % ptrs_per_blk;
        struct block *b;

        using_block(sb->dev, inode->blk_ptrs_triple[triple_blk], b)
            ret = ((uint32_t *)b->data)[double_blk];

        using_block(sb->dev, ret, b)
            ret = ((uint32_t *)b->data)[single_blk];

        using_block(sb->dev, ret, b)
            ret = ((uint32_t *)b->data)[direct_blk];

        goto return_sector;
    }

  return_sector:
    return ret;
}

