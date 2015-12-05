/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

/* This file includes everything related to the directory structure of an ext2
 * on-disk directory.
 *
 * IE. Finding entries, deleting entries, adding entries, etc. */

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

/* Finds the entry coresponding to 'name', and returns the on-disk entry in
 * '*result'. The block that *result resides in is returned by the function.
 * This block should be passed to brelease() when you're done messing with
 * *result. Note that this function may/will return NULL when a lookup fails */
static __must_check struct block *__ext2_lookup_entry(struct inode *dir, const char *name, size_t len, struct ext2_disk_directory_entry **result)
{
    struct block *b;
    int block_size = dir->sb->bdev->block_size;
    off_t cur_off;

    for (cur_off = 0; cur_off < dir->size; cur_off += block_size) {
        int offset = 0;
        struct ext2_disk_directory_entry *entry;
        sector_t sec = vfs_bmap(dir, cur_off / block_size);

        if (sec == SECTOR_INVALID)
            break;

        b = bread(dir->sb->dev, sec);

        for (offset = 0; offset < block_size && cur_off + offset < dir->size; offset += entry->rec_len) {
            entry = (struct ext2_disk_directory_entry *)(b->data + offset);
            if (entry->name_len_and_type[0]
                    && strncmp(name, entry->name, entry->name_len_and_type[0]) == 0) {
                *result = entry;
                return b;
            }
        }

        brelease(b);
    }

    return NULL;
}

int __ext2_dir_lookup(struct inode *dir, const char *name, size_t len, struct inode **result)
{
    struct block *b;
    struct ext2_disk_directory_entry *entry;

    b = __ext2_lookup_entry(dir, name, len, &entry);

    if (!b)
        return -ENOENT;

    *result = inode_get(dir->sb, entry->ino);

    brelease(b);

    return 0;
}

int __ext2_dir_add(struct inode *dir, const char *name, size_t len, ino_t ino, mode_t mode)
{
    return -ENOTSUP;
}

/* Marks the entry as empty by using inode-no zero */
int __ext2_dir_remove(struct inode *dir, const char *name, size_t len)
{
    struct block *b;
    struct ext2_disk_directory_entry *entry, *prev;
    int block_size = dir->sb->bdev->block_size;
    int offset;

    b = __ext2_lookup_entry(dir, name, len, &entry);

    if (!b)
        return -ENOENT;

    for (offset = 0, prev = NULL; offset < block_size; offset += prev->rec_len) {
        struct ext2_disk_directory_entry *new;
        new = (struct ext2_disk_directory_entry *)(b->data + offset);

        /* If we found our current entry, then 'prev' holds the previous entry,
         * if we have any. Increase the previous entries record length to
         * include 'entry' */
        if (new == entry) {
            if (prev)
                prev->rec_len += entry->rec_len;

            break;
        }

        prev = new;
    }

    entry->ino = 0;

    brelease(b);

    return 0;
}

int __ext2_dir_readdir(struct file *filp, struct file_readdir_handler *handler)
{
    struct block *b;
    struct inode *dir = filp->inode;
    int block_size = dir->sb->bdev->block_size;
    int ret = 0;
    int offset = 0;

    for (; filp->offset < dir->size && !ret; filp->offset += offset) {
        sector_t sec = vfs_bmap(dir, filp->offset / block_size);

        if (sec == SECTOR_INVALID)
            break;

        using_block(dir->sb->dev, sec, b) {
            int offset = 0;
            struct ext2_disk_directory_entry *entry;

            for (offset = 0; offset < b->block_size && filp->offset + offset < dir->size && !ret; offset += entry->rec_len) {
                entry = (struct ext2_disk_directory_entry *)(b->data + offset);

                ret = (handler->readdir) (handler, entry->ino, entry->name_len_and_type[1], entry->name, entry->name_len_and_type[0]);
            }
        }
    }

    return ret;
}

int __ext2_dir_read_dent(struct file *filp, struct dent *dent, size_t size)
{
    struct block *b;
    struct inode *dir = filp->inode;
    int block_size = dir->sb->bdev->block_size;
    int ret = 0;

  again:
    if (filp->offset == dir->size)
        return 0;

    sector_t sec = vfs_bmap(dir, filp->offset / block_size);
    int block_off = filp->offset % block_size;

    if (sec == SECTOR_INVALID)
        return -EINVAL;

    using_block(dir->sb->dev, sec, b) {
        struct ext2_disk_directory_entry *entry;

        entry = (struct ext2_disk_directory_entry *)(b->data + block_off);

        if (size < sizeof(struct dent) + entry->name_len_and_type[0] + 1) {
            ret = -EINVAL;
        } else {
            dent->ino = entry->ino;
            dent->dent_len = sizeof(struct dent) + entry->name_len_and_type[0] + 1;
            dent->name_len = entry->name_len_and_type[0];
            memcpy(dent->name, entry->name, entry->name_len_and_type[0]);
            dent->name[entry->name_len_and_type[0]] = '\0';

            ret = entry->rec_len;
            filp->offset += entry->rec_len;
        }
    }

    if (dent->ino == 0)
        goto again;

    return ret;
}

