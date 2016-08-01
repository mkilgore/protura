/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_INODE_TABLE_H
#define INCLUDE_FS_INODE_TABLE_H

#include <protura/types.h>
#include <protura/fs/super.h>

struct inode;

struct inode *inode_get(struct super_block *, ino_t ino);
struct inode *inode_dup(struct inode *);
void inode_put(struct inode *);
void inode_add(struct inode *i);

/* Called to reduce memory used by inode cache, by releasing inodes which have
 * no current users. */
void inode_oom(void);

#endif
