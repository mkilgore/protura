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
#include <mm/kmalloc.h>
#include <arch/task.h>

#include <fs/block.h>
#include <fs/super.h>
#include <fs/file.h>
#include <fs/stat.h>
#include <fs/inode.h>
#include <fs/inode_table.h>
#include <fs/vfs.h>

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
    dev_t dev = dir->dev;
    int sectors, i, ents;
    size_t sector_size = dir->sb->bdev->block_size;
    int dents_in_block = sector_size / sizeof(struct dirent);

    int found_entry = 0;
    struct dirent found;

    kprintf("Looking up %.*s\n", len, name);
    kprintf("Len: %d\n", len);

    if (!S_ISDIR(dir->mode))
        return -ENOTDIR;

    ents = dir->size / sizeof(struct dirent);
    sectors = (dir->size + sector_size - 1) / sector_size;

    kprintf("Dir: %p\n", dir);
    using_inode_lock_read(dir) {
        for (i = 0; i < sectors && !found_entry; i++) {
            sector_t s;
            int ents_to_check = (i * dents_in_block + dents_in_block > ents)
                                 ? ents - i * dents_in_block
                                 : dents_in_block;

            s = vfs_bmap(dir, i);

            using_block(dev, s, b)
                found_entry = check_ents_in_block(b, ents_to_check, name, len, &found);
        }
    }

    if (!found_entry)
        return -ENOENT;

    *result = inode_get(dir->sb, found.ino);

    return 0;
}

