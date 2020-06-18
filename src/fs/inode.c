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

static int check_ents_in_block(struct block *b, int ents, const char *name, size_t len, struct dirent *result)
{
    int k;
    struct dirent *dents = (struct dirent *)b->data;

    for (k = 0; k < ents; k++) {
        if (strlen(dents[k].name) == len) {
            if (strncmp(dents[k].name, name, len) == 0) {
                memcpy(result, dents + k, sizeof(struct dirent));
                *result = dents[k];
                return 1;
            }
        }
    }
    return 0;
}

/* Uses bmap to implement a generic lookup - Assumes every block consists of
 * 'struct dirent' objects. */
int inode_lookup_generic(struct inode *dir, const char *name, size_t len, struct inode **result)
{
    struct block *b;
    dev_t dev = dir->sb_dev;
    int sectors, i, ents;
    size_t sector_size = block_dev_get_block_size(dev);
    int dents_in_block = sector_size / sizeof(struct dirent);

    kp(KP_NORMAL, "inode lookup: Dev: %d:%d, block_size: %d\n", DEV_MAJOR(dev), DEV_MINOR(dev), sector_size);

    int found_entry = 0;
    struct dirent found;

    if (!S_ISDIR(dir->mode))
        return -ENOTDIR;

    ents = dir->size / sizeof(struct dirent);
    sectors = (dir->size + sector_size - 1) / sector_size;

    using_inode_lock_read(dir) {
        for (i = 0; i < sectors && !found_entry; i++) {
            sector_t s;
            int ents_to_check = (i * dents_in_block + dents_in_block > ents)
                                 ? ents - i * dents_in_block
                                 : dents_in_block;

            s = vfs_bmap(dir, i);

            using_block_locked(dev, s, b)
                found_entry = check_ents_in_block(b, ents_to_check, name, len, &found);
        }
    }

    if (!found_entry)
        return -ENOENT;

    *result = inode_get(dir->sb, found.ino);

    return 0;
}

struct inode_ops inode_ops_null = { };

