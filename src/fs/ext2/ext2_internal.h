/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef SRC_FS_EXT2_EXT2_INTERNAL_H
#define SRC_FS_EXT2_EXT2_INTERNAL_H

#include <protura/debug.h>
#include <fs/inode.h>
#include <fs/file.h>
#include <fs/ext2.h>

#ifdef CONFIG_KERNEL_LOG_EXT2
# define kp_ext2(sb, str, ...) kp(KP_DEBUG, "EXT2 (%p): " str, sb, ## __VA_ARGS__)
#else
# define kp_ext2(sb, str, ...) do { ; } while (0)
#endif

extern struct inode_ops ext2_inode_ops_dir;
extern struct inode_ops ext2_inode_ops_file;
extern struct file_ops ext2_file_ops_dir;
extern struct file_ops ext2_file_ops_file;

sector_t ext2_bmap(struct inode *i, sector_t inode_sector);
int __ext2_dir_lookup(struct inode *dir, const char *name, size_t len, struct inode **result);
int __ext2_dir_add(struct inode *dir, const char *name, size_t len, ino_t ino, mode_t mode);
int __ext2_dir_remove(struct inode *dir, const char *name, size_t len);
int __ext2_dir_readdir(struct file *filp, struct file_readdir_handler *handler);
int __ext2_dir_read_dent(struct file *filp, struct dent *dent, size_t size);

#endif
