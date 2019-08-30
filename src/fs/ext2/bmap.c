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

static sector_t __ext2_mark_block(struct ext2_super_block *sb)
{
    sector_t ret = SECTOR_INVALID;
    int blocks_per_group = sb->block_size * CHAR_BIT;
    int i;

    for (i = 0; i < sb->block_group_count; i++) {
        struct block *b;
        if (!sb->groups[i].block_unused_total)
            continue ;

        kp_ext2(sb, "Group: %d, block bitmap: %d\n", i, sb->groups[i].block_nr_block_bitmap);

        using_block(sb->sb.dev, sb->groups[i].block_nr_block_bitmap, b) {
            int location = bit_find_first_zero(b->data, sb->block_size);

            kp_ext2(sb, "First zero: %d\n", location);

            /* location is based from zero, but blocks start at 1 */
            ret = i * blocks_per_group + location + 1;
            bit_set(b->data, location);
            sb->groups[i].block_unused_total--;

            block_mark_dirty(b);
        }

        break;
    }

    return ret;
}

sector_t ext2_block_alloc(struct ext2_super_block *sb)
{
    sector_t ret = SECTOR_INVALID;

    using_super_block(&sb->sb) {
        ret = __ext2_mark_block(sb);

        if (ret != SECTOR_INVALID)
            sb->disksb.block_unused_total--;
    }

    kp_ext2(sb, "block_alloc: %d\n", ret);

    return ret;
}

void ext2_block_release(struct ext2_super_block *sb, sector_t orig_block)
{
    int blocks_per_group = sb->block_size * CHAR_BIT;
    int block = orig_block - 1;
    int group = block / blocks_per_group;
    int index = block % blocks_per_group;
    struct block *b;

    using_super_block(&sb->sb) {
        using_block(sb->sb.dev, sb->groups[group].block_nr_block_bitmap, b) {
            bit_clear(b->data, index);
            sb->groups[group].block_unused_total++;

            block_mark_dirty(b);
        }

        sb->disksb.block_unused_total++;
    }

    kp_ext2(sb, "block_release: %d\n", block);
}

static sector_t ext2_alloc_block_zero(struct ext2_super_block *sb)
{
    struct block *b;
    sector_t iblock = ext2_block_alloc(sb);

    using_block(sb->sb.dev, iblock, b) {
        memset(b->data, 0, b->block_size);
        block_mark_dirty(b);
    }

    return iblock;
}

static void ext2_map_indirect(struct ext2_super_block *sb, struct ext2_inode *inode, sector_t inode_sector, sector_t alloc_sector)
{
    const int ptrs_per_blk = sb->block_size / sizeof(uint32_t);
    int single_blk = inode_sector / ptrs_per_blk;
    int direct_blk = inode_sector % ptrs_per_blk;
    struct block *b;

    if (!inode->blk_ptrs_single[single_blk]) {
        inode->blk_ptrs_single[single_blk] = ext2_alloc_block_zero(sb);
        kp_ext2(sb, "Allocating new singly-indirect-block: %d\n", inode->blk_ptrs_single[single_blk]);
    }

    kp_ext2(sb, "Mapping to indirect block(idx %d) ptr idx %d -> %d\n", single_blk, direct_blk, alloc_sector);
    using_block(sb->sb.dev, inode->blk_ptrs_single[single_blk], b) {
        ((uint32_t *)b->data)[direct_blk] = alloc_sector;
        block_mark_dirty(b);
    }
}

static void ext2_map_dindirect(struct ext2_super_block *sb, struct ext2_inode *inode, sector_t inode_sector, sector_t alloc_sector)
{
    const int ptrs_per_blk = sb->block_size / sizeof(uint32_t);
    int double_blk = inode_sector / ptrs_per_blk / ptrs_per_blk;
    int single_blk = (inode_sector / ptrs_per_blk) % ptrs_per_blk;
    int direct_blk = inode_sector % ptrs_per_blk;
    struct block *b;
    sector_t indirect_block;

    if (!inode->blk_ptrs_double[double_blk]) {
        inode->blk_ptrs_double[double_blk] = ext2_alloc_block_zero(sb);
        kp_ext2(sb, "Mapping double-indirect ptr: %d\n", inode->blk_ptrs_double[double_blk]);
    }

    using_block(sb->sb.dev, inode->blk_ptrs_double[double_blk], b)
        indirect_block = ((uint32_t *)b->data)[single_blk];

    if (!indirect_block) {
        indirect_block = ext2_alloc_block_zero(sb);
        kp_ext2(sb, "Mapping d-indirect ptr: %d\n", indirect_block);

        using_block(sb->sb.dev, inode->blk_ptrs_double[double_blk], b) {
            ((uint32_t *)b->data)[single_blk] = indirect_block;
            block_mark_dirty(b);
        }
    }

    kp_ext2(sb, "Mapping to dindirect idx %d -> indirect idx %d -> direct idx %d\n", double_blk, single_blk, direct_blk);
    using_block(sb->sb.dev, indirect_block, b) {
        ((uint32_t *)b->data)[direct_blk] = alloc_sector;
        block_mark_dirty(b);
    }
}

static void ext2_map_tindirect(struct ext2_super_block *sb, struct ext2_inode *inode, sector_t inode_sector, sector_t alloc_sector)
{
    const int ptrs_per_blk = sb->block_size / sizeof(uint32_t);
    int triple_blk = inode_sector / ptrs_per_blk / ptrs_per_blk / ptrs_per_blk;
    int double_blk = (inode_sector / ptrs_per_blk / ptrs_per_blk) % ptrs_per_blk;
    int single_blk = (inode_sector / ptrs_per_blk) % ptrs_per_blk;;
    int direct_blk = inode_sector % ptrs_per_blk;
    struct block *b;
    sector_t dindrect_block, indirect_block;

    if (!inode->blk_ptrs_triple[triple_blk])
        inode->blk_ptrs_triple[triple_blk] = ext2_alloc_block_zero(sb);

    using_block(sb->sb.dev, inode->blk_ptrs_triple[triple_blk], b)
        dindrect_block = ((uint32_t *)b->data)[double_blk];

    if (!dindrect_block) {
        dindrect_block = ext2_alloc_block_zero(sb);

        using_block(sb->sb.dev, inode->blk_ptrs_triple[triple_blk], b) {
            ((uint32_t *)b->data)[double_blk] = dindrect_block;
            block_mark_dirty(b);
        }
    }

    using_block(sb->sb.dev, dindrect_block, b)
        indirect_block = ((uint32_t *)b->data)[single_blk];

    if (!indirect_block) {
        indirect_block = ext2_alloc_block_zero(sb);

        using_block(sb->sb.dev, dindrect_block, b) {
            ((uint32_t *)b->data)[single_blk] = indirect_block;
            block_mark_dirty(b);
        }
    }

    using_block(sb->sb.dev, indirect_block, b) {
        ((uint32_t *)b->data)[direct_blk] = alloc_sector;
        block_mark_dirty(b);
    }
}

sector_t ext2_bmap(struct inode *i, sector_t inode_sector)
{
    struct ext2_super_block *sb = container_of(i->sb, struct ext2_super_block, sb);
    const int ptrs_per_blk = sb->block_size / sizeof(uint32_t);
    struct ext2_inode *inode = container_of(i, struct ext2_inode, i);
    dev_t dev = sb->sb.dev;
    sector_t ret = SECTOR_INVALID;

    if (inode_sector < ARRAY_SIZE(inode->blk_ptrs_direct)) {
        ret = inode->blk_ptrs_direct[inode_sector];
        goto return_sector;
    }

    if (inode_sector > i->size / sb->block_size)
        return SECTOR_INVALID;

    inode_sector -= ARRAY_SIZE(inode->blk_ptrs_direct);

    if (inode_sector < ARRAY_SIZE(inode->blk_ptrs_single) * ptrs_per_blk) {
        int single_blk = inode_sector / ptrs_per_blk;
        int direct_blk = inode_sector % ptrs_per_blk;
        struct block *b;

        if (!inode->blk_ptrs_single[single_blk])
            goto return_sector;

        using_block(dev, inode->blk_ptrs_single[single_blk], b)
            ret = ((uint32_t *)b->data)[direct_blk];

        goto return_sector;
    }

    inode_sector -= ARRAY_SIZE(inode->blk_ptrs_single) * ptrs_per_blk;

    if (inode_sector < ARRAY_SIZE(inode->blk_ptrs_double) * ptrs_per_blk * ptrs_per_blk) {
        int double_blk = inode_sector / ptrs_per_blk / ptrs_per_blk;
        int single_blk = (inode_sector / ptrs_per_blk) % ptrs_per_blk;;
        int direct_blk = inode_sector % ptrs_per_blk;
        struct block *b;

        if (!inode->blk_ptrs_double[double_blk])
            goto return_sector;

        using_block(dev, inode->blk_ptrs_double[double_blk], b)
            ret = ((uint32_t *)b->data)[single_blk];

        if (!ret)
            goto return_sector;

        using_block(dev, ret, b)
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

        if (!inode->blk_ptrs_triple[triple_blk])
            goto return_sector;

        using_block(dev, inode->blk_ptrs_triple[triple_blk], b)
            ret = ((uint32_t *)b->data)[double_blk];

        if (!ret)
            goto return_sector;

        using_block(dev, ret, b)
            ret = ((uint32_t *)b->data)[single_blk];

        if (!ret)
            goto return_sector;

        using_block(dev, ret, b)
            ret = ((uint32_t *)b->data)[direct_blk];

        goto return_sector;
    }

  return_sector:
    if (ret == 0)
        return SECTOR_INVALID;
    return ret;
}

sector_t ext2_bmap_alloc(struct inode *inode, sector_t inode_sector)
{
    struct ext2_super_block *sb = container_of(inode->sb, struct ext2_super_block, sb);
    struct ext2_inode *i = container_of(inode, struct ext2_inode, i);
    sector_t ret = ext2_bmap(inode, inode_sector);

    kp_ext2(sb, "bmap ret: %d\n", ret);

    if (ret != SECTOR_INVALID)
        return ret;

    ret = ext2_alloc_block_zero(sb);

    kp_ext2(sb, "Alloced block: %d -> %d\n", inode_sector, ret);

    /* FIXME: This new block needs to be mapped back into the inode correctly. */
    if (inode_sector < ARRAY_SIZE(i->blk_ptrs_direct)) {
        kp(KP_TRACE, "Mapping allocated block to direct ptrs\n");
        i->blk_ptrs_direct[inode_sector] = ret;
    } else {
        const int ptrs_per_blk = sb->block_size / sizeof(uint32_t);

        inode_sector -= ARRAY_SIZE(i->blk_ptrs_direct);

        if (inode_sector < ARRAY_SIZE(i->blk_ptrs_single) * ptrs_per_blk) {
            ext2_map_indirect(sb, i, inode_sector, ret);
        } else {
            inode_sector -= ARRAY_SIZE(i->blk_ptrs_single) * ptrs_per_blk;

            if (inode_sector < ARRAY_SIZE(i->blk_ptrs_double) * ptrs_per_blk * ptrs_per_blk) {
                ext2_map_dindirect(sb, i, inode_sector, ret);
            } else {
                inode_sector -= ARRAY_SIZE(i->blk_ptrs_double) * ptrs_per_blk * ptrs_per_blk;

                if (inode_sector < ARRAY_SIZE(i->blk_ptrs_triple) * ptrs_per_blk * ptrs_per_blk * ptrs_per_blk) {
                    ext2_map_tindirect(sb, i, inode_sector, ret);
                } else {
                    ext2_block_release(sb, ret);
                    kp(KP_ERROR, "Error: File too large!\n");

                    return SECTOR_INVALID;
                }
            }
        }
    }

    i->i.blocks += (sb->block_size / 512);
    inode_set_dirty(inode);

    return ret;
}

