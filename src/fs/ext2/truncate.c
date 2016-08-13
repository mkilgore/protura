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
#include <protura/fs/vfs.h>
#include <protura/fs/ext2.h>
#include "ext2_internal.h"

/* Fairly simple algorithm. 'size' is aligned to the next highest block, as is
 * the current inode size. The two are subtracted and the difference is used to
 * calcluate the total number of blocks to be discarded. From there, 'size' is
 * used to calculate the first block to discard, and then we just loop. There
 * is complexity from the single/double/triple indirect blocks, but not too
 * bad.
 */
#define BLOCKS_IN_INDIRECT(block_size) ((block_size) / 4)
#define BLOCKS_IN_DINDIRECT(block_size) ((BLOCKS_IN_INDIRECT(block_size)) * (BLOCKS_IN_INDIRECT(block_size)))
#define BLOCKS_IN_TINDIRECT(block_size) ((BLOCKS_IN_INDIRECT(block_size)) * (BLOCKS_IN_DINDIRECT(block_size)))

static int __ext2_inode_truncate_direct(struct ext2_inode *inode, struct ext2_super_block *s, int starting_block, int ending_block)
{
    int i;

    for (i = starting_block; i < ending_block && i < 12; i++) {
        if (inode->blk_ptrs_direct[i])
            ext2_block_release(s, inode->blk_ptrs_direct[i]);
        inode->blk_ptrs_direct[i] = 0;
    }

    return 0;
}

static int __ext2_inode_truncate_sindirect(struct ext2_inode *inode,
                                           struct ext2_super_block *s,
                                           struct block *b,
                                           int block_offset,
                                           int starting_block,
                                           int ending_block,
                                           int block_size)
{
    int i;
    int starting_offset = starting_block - block_offset;
    int ending_offset = ending_block - block_offset;

    if (starting_offset < 0)
        starting_offset = 0;

    if (ending_offset > BLOCKS_IN_INDIRECT(block_size))
        ending_offset = BLOCKS_IN_INDIRECT(block_size);

    for (i = starting_offset; i < ending_offset; i++) {
        uint32_t addr = ((uint32_t *)(b->data))[i];

        /* Sparse files means that not every position has a block, even if the
         * ones before and after it do. This applies at all levels. */
        if (addr)
            ext2_block_release(s, addr);

        ((uint32_t *)(b->data))[i] = 0;
        b->dirty = 1;
    }

    return 0;
}

static int __ext2_inode_truncate_dindirect(struct ext2_inode *inode,
                                           struct ext2_super_block *s,
                                           struct block *b,
                                           int block_offset,
                                           int starting_block,
                                           int ending_block,
                                           int block_size)
{
    int i;
    int starting_offset, ending_offset;

    starting_offset = (starting_block - block_offset) / BLOCKS_IN_INDIRECT(block_size);
    ending_offset = (ending_block - block_offset) / BLOCKS_IN_INDIRECT(block_size);

    if (starting_offset < 0)
        starting_offset = 0;

    if (ending_offset > BLOCKS_IN_INDIRECT(block_size))
        ending_offset = BLOCKS_IN_INDIRECT(block_size);

    for (i = starting_offset; i < ending_offset; i++) {
        uint32_t addr = ((uint32_t *)(b->data))[i];

        if (addr) {
            struct block *new_b;
            int s_block_offset = i * BLOCKS_IN_INDIRECT(block_size) + block_offset;

            using_block(s->sb.dev, addr, new_b)
                __ext2_inode_truncate_sindirect(inode, s, new_b, s_block_offset, starting_block, ending_block, block_size);

            if (s_block_offset >= starting_block) {
                ext2_block_release(s, addr);
                ((uint32_t *)(b->data))[i] = 0;
            }
        }
    }

    return 0;
}

static int __ext2_inode_truncate_tindirect(struct ext2_inode *inode,
                                           struct ext2_super_block *s,
                                           struct block *b,
                                           int block_offset,
                                           int starting_block,
                                           int ending_block,
                                           int block_size)
{
    return 0;
}

int __ext2_inode_truncate(struct ext2_inode *inode, off_t size)
{
    struct ext2_super_block *sb = container_of(inode->i.sb, struct ext2_super_block, sb);
    int block_size = sb->block_size;
    int starting_block, ending_block;
    int ret = 0;
    struct block *b;

    starting_block = ALIGN_2(size, block_size) / block_size;
    ending_block = ALIGN_2_DOWN(inode->i.size, block_size) / block_size;

    if (starting_block >= ending_block)
        goto set_size_and_ret;

    if (ending_block > 12 + BLOCKS_IN_INDIRECT(block_size))
        panic("Size: %d, Ending block: %d, Truncating files that large not supported!\n", inode->i.size, ending_block);

    if (starting_block < 12) {
        ret = __ext2_inode_truncate_direct(inode, sb, starting_block, ending_block);
        if (ret)
            return ret;
    }

    if (starting_block < 12 + BLOCKS_IN_INDIRECT(block_size)) {
        using_block(sb->sb.dev, inode->blk_ptrs_single[0], b)
            ret = __ext2_inode_truncate_sindirect(inode, sb, b, 12, starting_block, ending_block, block_size);

        if (ret)
            return ret;

        if (starting_block < 12) {
            ext2_block_release(sb, inode->blk_ptrs_single[0]);
            inode->blk_ptrs_single[0] = 0;
        }
    }

    /* If size does not end on a block boundary, then it is required to clear
     * the last block till the end with zeros. */
    if ((size % block_size) != 0) {
        using_block(sb->sb.dev, vfs_bmap(&inode->i, starting_block), b) {
            memset(b->data + (size % block_size), 0, block_size - (size % block_size));
            b->dirty = 1;
        }
    }

  set_size_and_ret:
    /* 'blocks' is always a count of 512-byte blocks */
    inode->i.blocks = starting_block * (block_size / 512);
    inode->i.size = size;
    inode_set_dirty(&inode->i);
    return 0;
}

int ext2_truncate(struct inode *inode, off_t size)
{
    struct ext2_inode *i = container_of(inode, struct ext2_inode, i);
    int ret;

    using_inode_lock_write(inode)
        ret = __ext2_inode_truncate(i, size);

    return ret;
}

