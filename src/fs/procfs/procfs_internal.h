/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_PROCFS_PROCFS_INTERNAL_H
#define INCLUDE_FS_PROCFS_PROCFS_INTERNAL_H

#include <protura/fs/inode.h>
#include <protura/fs/file.h>
#include <protura/fs/procfs.h>

#define PROCFS_ROOT_INO 1

ino_t procfs_next_ino(void);

void procfs_hash_add_node(struct procfs_node *node);
struct procfs_node *procfs_hash_get_node(ino_t ino);

extern struct inode_ops procfs_dir_inode_ops;
extern struct file_ops procfs_dir_file_ops;

extern struct inode_ops procfs_file_inode_ops;
extern struct file_ops procfs_file_file_ops;

#endif
