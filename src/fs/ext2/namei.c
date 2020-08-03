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
#include <protura/block/bcache.h>
#include <protura/fs/char.h>
#include <protura/fs/stat.h>
#include <protura/fs/file.h>
#include <protura/fs/file_system.h>
#include <protura/fs/vfs.h>
#include <protura/fs/ext2.h>
#include "ext2_internal.h"

static size_t ext2_disk_dir_entry_rec_len(size_t name_len)
{
    return ALIGN_2(name_len, 4) + sizeof(struct ext2_disk_directory_entry);
}

uint8_t ext2_dt_to_dir_type[] = {
    [DT_UNKNOWN] = EXT2_FT_UNKNOWN,
    [DT_REG] = EXT2_FT_REG_FILE,
    [DT_DIR] = EXT2_FT_DIR,
    [DT_CHR] = EXT2_FT_CHRDEV,
    [DT_BLK] = EXT2_FT_BLKDEV,
    [DT_LNK] = EXT2_FT_SYMLINK,
    [DT_FIFO] = EXT2_FT_FIFO,
    /*
    [DT_SOCK] = EXT2_FT_SOCK,
     */
};

uint8_t ext2_dir_type_to_dt[] = {
    [EXT2_FT_UNKNOWN] = DT_UNKNOWN,
    [EXT2_FT_REG_FILE] = DT_REG,
    [EXT2_FT_DIR] = DT_DIR,
    [EXT2_FT_CHRDEV] = DT_CHR,
    [EXT2_FT_BLKDEV] = DT_BLK,
    [EXT2_FT_SYMLINK] = DT_LNK,
    [EXT2_FT_FIFO] = DT_FIFO,
};

static uint8_t ext2_dir_type(mode_t mode)
{
    return ext2_dt_to_dir_type[MODE_TO_DT(mode)];
}

/* Finds the entry coresponding to 'name', and returns the on-disk entry in
 * '*result'. The block that *result resides in is returned by the function.
 * This block should be passed to brelease() when you're done messing with
 * *result. Note that this function may/will return NULL when a lookup fails */
__must_check struct block *__ext2_lookup_entry(struct inode *dir, const char *name, size_t len, struct ext2_disk_directory_entry **result)
{
    struct ext2_super_block *sb = container_of(dir->sb, struct ext2_super_block, sb);
    struct block *b;
    int block_size = sb->block_size;
    off_t cur_off;

    for (cur_off = 0; cur_off < dir->size; cur_off += block_size) {
        int offset = 0;
        struct ext2_disk_directory_entry *entry;
        sector_t sec = vfs_bmap(dir, cur_off / block_size);

        kp_ext2(dir->sb, "Lookup block size: %d, cur_off: %ld, Sec: %d\n", block_size, cur_off, sec);

        if (sec == SECTOR_INVALID)
            break;

        b = block_getlock(dir->sb->bdev, sec);

        for (offset = 0; offset < block_size && cur_off + offset < dir->size; offset += entry->rec_len) {
            entry = (struct ext2_disk_directory_entry *)(b->data + offset);
            if (entry->name_len_and_type[EXT2_DENT_NAME_LEN_LOW]
                    && entry->name_len_and_type[EXT2_DENT_NAME_LEN_LOW] == len
                    && strncmp(name, entry->name, entry->name_len_and_type[EXT2_DENT_NAME_LEN_LOW]) == 0) {
                *result = entry;
                return b;
            }
        }

        block_unlockput(b);
    }

    return NULL;
}

__must_check struct block *__ext2_add_entry(struct inode *dir, const char *name, size_t len, struct ext2_disk_directory_entry **result, int *err)
{
    struct block *b;
    struct ext2_super_block *sb = container_of(dir->sb, struct ext2_super_block, sb);
    int block_size = sb->block_size;
    off_t cur_off;
    size_t minimum_rec_len = ext2_disk_dir_entry_rec_len(len);
    sector_t sec;
    struct ext2_disk_directory_entry *entry;

    kp_ext2(dir->sb, "add entry: %s\n", name);

    for (cur_off = 0; cur_off < dir->size; cur_off += block_size) {
        int offset = 0;

        sec = vfs_bmap(dir, cur_off / block_size);

        if (sec == SECTOR_INVALID)
            break;

        b = block_getlock(dir->sb->bdev, sec);

        for (offset = 0; offset < block_size && cur_off + offset < dir->size; offset += entry->rec_len) {
            size_t entry_minimum_rec_len;
            entry = (struct ext2_disk_directory_entry *)(b->data + offset);

            entry_minimum_rec_len = ext2_disk_dir_entry_rec_len(entry->name_len_and_type[EXT2_DENT_NAME_LEN_LOW]);

            if (entry->ino == 0 && entry->rec_len >= minimum_rec_len) {
                entry->name_len_and_type[EXT2_DENT_NAME_LEN_LOW] = len;
                memcpy(entry->name, name, len);
                goto return_entry;
            } else if (entry->rec_len >= minimum_rec_len + entry_minimum_rec_len) {
                size_t new_rec_len = entry->rec_len - entry_minimum_rec_len;
                entry->rec_len = entry_minimum_rec_len;

                entry = (struct ext2_disk_directory_entry *)((char *)entry + entry_minimum_rec_len);
                entry->ino = 0;
                entry->rec_len = new_rec_len;
                entry->name_len_and_type[EXT2_DENT_NAME_LEN_LOW] = len;

                memcpy(entry->name, name, len);
                goto return_entry;
            }
        }

        block_unlockput(b);
    }

    kp_ext2(dir->sb, "Truncating directory: %ld\n", dir->size + block_size);

    __ext2_inode_truncate(container_of(dir, struct ext2_inode, i), dir->size + block_size);

    kp_ext2(dir->sb, "bmap_alloc\n");
    sec = ext2_bmap_alloc(dir, (dir->size - 1) / block_size);

    kp_ext2(dir->sb, "dir->size=%ld\n", dir->size);
    kp_ext2(dir->sb, "sec=%d\n", sec);
    if (sec == SECTOR_INVALID) {
        *err = -ENOSPC;
        return NULL;
    }

    b = block_getlock(dir->sb->bdev, sec);
    entry = (struct ext2_disk_directory_entry *)b->data;
    entry->ino = 0;
    entry->rec_len = block_size;
    entry->name_len_and_type[EXT2_DENT_NAME_LEN_LOW] = len;
    memcpy(entry->name, name, len);

  return_entry:
    block_mark_dirty(b);

    *result = entry;
    *err = 0;
    return b;
}

int __ext2_dir_lookup_ino(struct inode *dir, const char *name, size_t len, ino_t *ino)
{
    struct block *b;
    struct ext2_disk_directory_entry *entry;

    b = __ext2_lookup_entry(dir, name, len, &entry);

    if (!b)
        return -ENOENT;

    *ino = entry->ino;

    kp_ext2(dir->sb, "DIR Ent, ino: %d, name: %s\n", entry->ino, entry->name);

    block_unlockput(b);

    return 0;
}

int ext2_dir_lookup(struct inode *dir, const char *name, size_t len, struct inode **result)
{
    ino_t entry = 0;
    int ret;

    kp_ext2(dir->sb, "Lookup: "PRinode" name: %s, len: %d\n", Pinode(dir), name, len);

    using_inode_lock_read(dir) {
        ret = __ext2_dir_lookup_ino(dir, name, len, &entry);
        if (ret)
            return ret;
    }

    kp_ext2(dir->sb, "Lookup inode: %d\n", entry);
    *result = inode_get(dir->sb, entry);

    if (!*result)
        return -ENOENT;

    return 0;
}

/* Returns true if the entry 'name' exists in 'dir' */
int __ext2_dir_entry_exists(struct inode *dir, const char *name, size_t len)
{
    struct block *b;
    struct ext2_disk_directory_entry *entry;

    b = __ext2_lookup_entry(dir, name, len, &entry);

    if (!b)
        return 0;

    block_unlockput(b);

    return 1;
}

/* Note: This does not check for duplicates. If a duplicate is a possibility,
 * call __ext2_dir_entry_exists() first. */
int __ext2_dir_add(struct inode *dir, const char *name, size_t len, ino_t ino, mode_t mode)
{
    struct block *b;
    struct ext2_disk_directory_entry *entry;
    int err;

    b = __ext2_add_entry(dir, name, len, &entry, &err);

    if (!b)
        return err;

    entry->ino = ino;
    entry->name_len_and_type[EXT2_DENT_TYPE] = ext2_dir_type(mode);

    block_unlockput(b);

    return 0;
}

int __ext2_dir_remove_entry(struct inode *dir, struct block *b, struct ext2_disk_directory_entry *entry)
{
    struct ext2_disk_directory_entry *prev;
    struct ext2_super_block *sb = container_of(dir->sb, struct ext2_super_block, sb);
    int block_size = sb->block_size;
    int offset;

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

    block_mark_dirty(b);

    return 0;
}

/*
 * Reports 0 when the directory contains no entries other then '.' and '..'
 * Else reports the error (ENOTEMPTY)
 */
int __ext2_dir_empty(struct inode *dir)
{
    struct ext2_super_block *sb = container_of(dir->sb, struct ext2_super_block, sb);
    int ret = 0;
    int block_size = sb->block_size;
    int blocks;
    int cur_block;

    blocks = ALIGN_2(dir->size, block_size) / block_size;

    for (cur_block = 0; cur_block < blocks && !ret; cur_block++) {
        sector_t sec;
        struct block *b;

        sec = vfs_bmap(dir, cur_block);

        if (sec == SECTOR_INVALID)
            return -ENOTEMPTY;

        using_block_locked(dir->sb->bdev, sec, b) {
            int offset;
            struct ext2_disk_directory_entry *new;

            for (offset = 0, new = NULL; offset < block_size; offset += new->rec_len) {
                size_t len;
                new = (struct ext2_disk_directory_entry *)(b->data + offset);

                len = new->name_len_and_type[EXT2_DENT_NAME_LEN_LOW];

                /* Entries with inode 0 are fine, they are empty */
                if (new->ino == 0)
                    continue;

                /* The only allowed entries are '.' and '..' */
                if (len == 0) {
                    ret = -ENOTEMPTY;
                    break;
                }

                if (len > 2) {
                    ret = -ENOTEMPTY;
                    break;
                }

                if (new->name[0] != '.') {
                    ret = -ENOTEMPTY;
                    break;
                }

                if (len == 1)
                    continue;

                if (new->name[1] != '.') {
                    ret = -ENOTEMPTY;
                    break;
                }
            }
        }
    }

    return ret;
}

int __ext2_dir_change_dotdot(struct inode *dir, ino_t ino)
{
    struct block *b;
    struct ext2_disk_directory_entry *entry;

    b = __ext2_lookup_entry(dir, "..", 2, &entry);
    if (!b)
        return -ENOENT;

    entry->ino = ino;

    block_mark_dirty(b);
    block_unlockput(b);

    return 0;
}

/* Marks the entry as empty by using inode-no zero, and combining it into the
 * previous inode entry. */
int __ext2_dir_remove(struct inode *dir, const char *name, size_t len)
{
    int ret;
    struct block *b;
    struct ext2_disk_directory_entry *entry;

    b = __ext2_lookup_entry(dir, name, len, &entry);

    if (!b)
        return -ENOENT;

    ret = __ext2_dir_remove_entry(dir, b, entry);

    block_unlockput(b);

    return ret;
}

int __ext2_dir_readdir(struct file *filp, struct file_readdir_handler *handler)
{
    struct block *b;
    struct inode *dir = filp->inode;
    struct ext2_super_block *sb = container_of(dir->sb, struct ext2_super_block, sb);
    int block_size = sb->block_size;
    int ret = 0;
    int offset = 0;

    for (; filp->offset < dir->size && !ret; filp->offset += offset) {
        sector_t sec = vfs_bmap(dir, filp->offset / block_size);

        if (sec == SECTOR_INVALID)
            break;

        using_block_locked(dir->sb->bdev, sec, b) {
            int offset = 0;
            struct ext2_disk_directory_entry *entry;

            for (offset = 0; offset < b->block_size && filp->offset + offset < dir->size && !ret; offset += entry->rec_len) {
                entry = (struct ext2_disk_directory_entry *)(b->data + offset);

                ret = (handler->readdir) (handler, entry->ino, entry->name_len_and_type[EXT2_DENT_TYPE], entry->name, entry->name_len_and_type[EXT2_DENT_NAME_LEN_LOW]);
            }
        }
    }

    return ret;
}

int __ext2_dir_read_dent(struct file *filp, struct user_buffer dent, size_t size)
{
    struct block *b;
    struct inode *dir = filp->inode;
    struct ext2_super_block *sb = container_of(dir->sb, struct ext2_super_block, sb);
    int block_size = sb->block_size;
    int ret = 0;
    int skip;

    kp_ext2(dir->sb, "Dir size: %ld\n", dir->size);

  again:
    skip = 0;

    if (filp->offset == dir->size)
        return 0;

    sector_t sec = vfs_bmap(dir, filp->offset / block_size);
    int block_off = filp->offset % block_size;

    if (sec == SECTOR_INVALID) {
        kp_ext2(dir->sb, "invalid sector\n");
        return -EINVAL;
    }

    using_block_locked(dir->sb->bdev, sec, b) {
        struct ext2_disk_directory_entry *entry;

        entry = (struct ext2_disk_directory_entry *)(b->data + block_off);

        if (entry->ino == 0) {
            filp->offset += entry->rec_len;
            skip = 1;
            break;
        }

        if (size < sizeof(struct dent) + entry->name_len_and_type[EXT2_DENT_NAME_LEN_LOW] + 1) {
            ret = -EINVAL;
        } else {
            ret = user_copy_dent(dent,
                                 entry->ino,
                                 sizeof(struct dent) + entry->name_len_and_type[EXT2_DENT_NAME_LEN_LOW] + 1,
                                 entry->name_len_and_type[EXT2_DENT_NAME_LEN_LOW],
                                 entry->name);

            if (!ret) {
                ret = entry->rec_len;
                filp->offset += entry->rec_len;
            }
        }
    }

    if (skip)
        goto again;

    return ret;
}

