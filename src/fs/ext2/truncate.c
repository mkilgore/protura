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

#define PTRS_PER_BLOCK(block_size) ((block_size) / 4)
#define BLOCKS_IN_INDIRECT(block_size) ((block_size) / 4)
#define BLOCKS_IN_DINDIRECT(block_size) ((BLOCKS_IN_INDIRECT(block_size)) * (BLOCKS_IN_INDIRECT(block_size)))
#define BLOCKS_IN_TINDIRECT(block_size) ((BLOCKS_IN_INDIRECT(block_size)) * (BLOCKS_IN_DINDIRECT(block_size)))

/* Used to hold sectors being freed.
 * This exists so that we don't have to keep blocks locked when we call
 * into other functions, prevent possible deadlocks. */
struct tmp_blocks {
    uint32_t *ptrs;
    int next_ptr;
};

struct truncate_state {
    struct ext2_inode *inode;
    struct ext2_super_block *sb;
    off_t block_size;
    int cleared_block_count;

    struct tmp_blocks tmp_indirect;
    struct tmp_blocks tmp_dindirect;
    struct tmp_blocks tmp_tindirect;
};

static void __tmp_blocks_add_block(struct tmp_blocks *blocks, uint32_t block_addr)
{
    if (block_addr)
        blocks->ptrs[blocks->next_ptr++] = block_addr;
}

static void __tmp_blocks_release_blocks(struct ext2_super_block *sb, struct ext2_inode *inode, struct tmp_blocks *blocks)
{
    int i;
    for (i = 0; i < blocks->next_ptr; i++)
        ext2_block_release(sb, inode, blocks->ptrs[i]);

    blocks->next_ptr = 0;
}

static int __ext2_inode_truncate_indirect_page(struct truncate_state *state, sector_t addr, int starting_block)
{
    struct block *b;
    int i;

    using_block(state->sb->sb.dev, addr, b) {
        uint32_t *ptrs = (uint32_t *)b->data;

        for (i = starting_block; i < PTRS_PER_BLOCK(state->block_size); i++) {
            __tmp_blocks_add_block(&state->tmp_indirect, ptrs[i]);
            ptrs[i] = 0;
        }

        block_mark_dirty(b);
    }

    __tmp_blocks_release_blocks(state->sb, state->inode, &state->tmp_indirect);
    return 0;
}

static int __ext2_inode_truncate_dindirect_page(struct truncate_state *state, sector_t addr, int starting_block, int starting_direct_block)
{
    struct block *b;
    int i;

    using_block(state->sb->sb.dev, addr, b) {
        uint32_t *ptrs = (uint32_t *)b->data;

        for (i = starting_block; i < PTRS_PER_BLOCK(state->block_size); i++) {
            __tmp_blocks_add_block(&state->tmp_dindirect, ptrs[i]);
            ptrs[i] = 0;
        }

        block_mark_dirty(b);
    }

    for (i = 0; i < state->tmp_dindirect.next_ptr; i++) {
        if (state->tmp_dindirect.ptrs[i])
            __ext2_inode_truncate_indirect_page(state, state->tmp_dindirect.ptrs[i], starting_direct_block);
        starting_direct_block = 0;
    }

    __tmp_blocks_release_blocks(state->sb, state->inode, &state->tmp_dindirect);
    return 0;
}

static int __ext2_inode_truncate_tindirect_page(struct truncate_state *state, sector_t addr, int starting_block, int starting_indirect_block, int starting_direct_block)
{
    struct block *b;
    int i;

    using_block(state->sb->sb.dev, addr, b) {
        uint32_t *ptrs = (uint32_t *)b->data;

        for (i = starting_block; i < PTRS_PER_BLOCK(state->block_size); i++) {
            __tmp_blocks_add_block(&state->tmp_tindirect, ptrs[i]);
            ptrs[i] = 0;
        }

        block_mark_dirty(b);
    }

    for (i = 0; i < state->tmp_tindirect.next_ptr; i++) {
        if (state->tmp_tindirect.ptrs[i])
            __ext2_inode_truncate_dindirect_page(state, state->tmp_tindirect.ptrs[i], starting_indirect_block, starting_direct_block);
        starting_indirect_block = 0;
        starting_direct_block = 0;
    }

    __tmp_blocks_release_blocks(state->sb, state->inode, &state->tmp_tindirect);
    return 0;
}

static void __ext2_inode_truncate_remove(struct ext2_super_block *sb, struct ext2_inode *inode, sector_t starting_block)
{
    const int block_size = sb->block_size;
    const int ptrs_per_blk = PTRS_PER_BLOCK(block_size);
    struct truncate_state trunc_state;

    memset(&trunc_state, 0, sizeof(trunc_state));

    trunc_state.inode = inode;
    trunc_state.sb = sb;
    trunc_state.block_size = block_size;

    trunc_state.tmp_indirect.ptrs = palloc_va(0, PAL_KERNEL);
    trunc_state.tmp_dindirect.ptrs = palloc_va(0, PAL_KERNEL);
    trunc_state.tmp_tindirect.ptrs = palloc_va(0, PAL_KERNEL);

    if (starting_block < ARRAY_SIZE(inode->blk_ptrs_direct)) {
        sector_t i;

        for (i = starting_block; i < ARRAY_SIZE(inode->blk_ptrs_direct); i++) {
            if (inode->blk_ptrs_direct[i]) {
                ext2_block_release(sb, inode, inode->blk_ptrs_direct[i]);
                inode->blk_ptrs_direct[i] = 0;
            }
        }

        starting_block = 0;
    } else {
        starting_block -= ARRAY_SIZE(inode->blk_ptrs_direct);
    }

    if (starting_block < BLOCKS_IN_INDIRECT(block_size)) {
        if (inode->blk_ptrs_single[0]) {
            __ext2_inode_truncate_indirect_page(&trunc_state, inode->blk_ptrs_single[0], starting_block);
            if (starting_block == 0) {
                ext2_block_release(sb, inode, inode->blk_ptrs_single[0]);
                inode->blk_ptrs_single[0] = 0;
            }
        }
        starting_block = 0;
    } else {
        starting_block -= BLOCKS_IN_INDIRECT(block_size);
    }

    if (starting_block < BLOCKS_IN_DINDIRECT(block_size)) {
        sector_t idblock = starting_block / ptrs_per_blk;
        sector_t dblock = starting_block % ptrs_per_blk;

        if (inode->blk_ptrs_double[0]) {
            __ext2_inode_truncate_dindirect_page(&trunc_state, inode->blk_ptrs_double[0], idblock, dblock);
            if (starting_block == 0) {
                ext2_block_release(sb, inode, inode->blk_ptrs_double[0]);
                inode->blk_ptrs_double[0] = 0;
            }
        }

        starting_block = 0;
    } else {
        starting_block -= BLOCKS_IN_DINDIRECT(block_size);
    }

    if (starting_block < BLOCKS_IN_TINDIRECT(block_size)) {
        sector_t didblock = starting_block / ptrs_per_blk / ptrs_per_blk;
        sector_t idblock = (starting_block / ptrs_per_blk) % ptrs_per_blk;
        sector_t dblock = starting_block / ptrs_per_blk / ptrs_per_blk / ptrs_per_blk;

        if (inode->blk_ptrs_triple[0]) {
            __ext2_inode_truncate_tindirect_page(&trunc_state, inode->blk_ptrs_triple[0], didblock, idblock, dblock);
            if (starting_block == 0) {
                ext2_block_release(sb, inode, inode->blk_ptrs_triple[0]);
                inode->blk_ptrs_triple[0] = 0;
            }
        }
    }

    pfree_va(trunc_state.tmp_indirect.ptrs, 0);
    pfree_va(trunc_state.tmp_dindirect.ptrs, 0);
    pfree_va(trunc_state.tmp_tindirect.ptrs, 0);
}

int __ext2_inode_truncate(struct ext2_inode *inode, off_t size)
{
    struct ext2_super_block *sb = container_of(inode->i.sb, struct ext2_super_block, sb);
    int block_size = sb->block_size;
    sector_t starting_block, ending_block;
    struct block *b;

    starting_block = ALIGN_2(size, block_size) / block_size;
    ending_block = ALIGN_2_DOWN(inode->i.size, block_size) / block_size;

    if (starting_block > ending_block)
        goto set_size_and_ret;

    __ext2_inode_truncate_remove(sb, inode, starting_block);

    /* If size does not end on a block boundary, then it is required to clear
     * the last block till the end with zeros. */
    if ((size % block_size) != 0) {
        using_block(sb->sb.dev, vfs_bmap(&inode->i, ALIGN_2_DOWN(size, block_size)), b) {
            memset(b->data + (size % block_size), 0, block_size - (size % block_size));
            block_mark_dirty(b);
        }
    }

  set_size_and_ret:
    inode->i.size = size;
    inode->i.ctime = inode->i.mtime = protura_current_time_get();
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

