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
#include <protura/block/bcache.h>
#include <protura/fs/char.h>
#include <protura/fs/stat.h>
#include <protura/fs/file.h>
#include <protura/fs/file_system.h>
#include <protura/fs/vfs.h>
#include <protura/fs/namei.h>
#include <protura/fs/ext2.h>
#include "ext2_internal.h"

static int ext2_follow_link(struct inode *dir, struct inode *symlink, struct inode **result)
{
    struct ext2_inode *ext2_symlink = container_of(symlink, struct ext2_inode, i);
    struct ext2_super_block *sb = container_of(symlink->sb, struct ext2_super_block, sb);
    struct block *b = NULL;
    char *link;
    int ret = 0;

    kp_ext2(sb, "In follow_link\n");

    if (!S_ISLNK(symlink->mode)) {
        *result = inode_dup(symlink);
        return 0;
    }

    /* inode is only locked in the event we need to access its blocks.
     *
     * The locking is probably still overkill, because the contents of symlinks
     * can't be changed, so in theory even if we didn't take the lock this
     * would still work fine.
     */
    if (ext2_symlink->i.blocks) {
        inode_lock_read(symlink);

        sector_t s = ext2_bmap(&ext2_symlink->i, 0);
        if (s == SECTOR_INVALID)
            return -EINVAL;

        b = block_getlock(ext2_symlink->i.sb->bdev, s);
        if (!b)
            return -EINVAL;

        link = b->data;
    } else {
        link = (char *)ext2_symlink->blk_ptrs;
    }

    ret = namex(link, dir, result);

    symlink->atime = protura_current_time_get();
    inode_set_dirty(symlink);

    if (b) {
        block_unlockput(b);
        inode_unlock_read(symlink);
    }

    return ret;
}

static int ext2_readlink(struct inode *symlink, char *buf, size_t buf_len)
{
    struct ext2_inode *ext2_symlink = container_of(symlink, struct ext2_inode, i);
    struct block *b = NULL;
    char *link;

    if (!S_ISLNK(symlink->mode))
        return -EINVAL;

    if (ext2_symlink->i.blocks) {
        sector_t s = ext2_bmap(&ext2_symlink->i, 0);
        if (s == SECTOR_INVALID)
            return -EINVAL;

        b = block_getlock(ext2_symlink->i.sb->bdev, s);
        if (!b)
            return -EINVAL;

        link = b->data;
    } else {
        link = (char *)ext2_symlink->blk_ptrs;
    }

    strncpy(buf, link, buf_len);
    buf[buf_len - 1] = '\0';

    symlink->atime = protura_current_time_get();
    inode_set_dirty(symlink);

    if (b)
        block_unlockput(b);
    return 0;
}

struct inode_ops ext2_inode_ops_symlink = {
    .follow_link = ext2_follow_link,
    .readlink = ext2_readlink,
};


