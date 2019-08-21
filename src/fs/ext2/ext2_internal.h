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
#include <protura/fs/inode.h>
#include <protura/fs/file.h>
#include <protura/fs/ext2.h>

#ifdef CONFIG_KERNEL_LOG_EXT2
# define kp_ext2(sb, str, ...) kp(KP_DEBUG, "EXT2 (%p): " str, sb, ## __VA_ARGS__)
#else
# define kp_ext2(sb, str, ...) do { (void)(sb); } while (0)
#endif

extern struct inode_ops ext2_inode_ops_dir;
extern struct inode_ops ext2_inode_ops_file;
extern struct file_ops ext2_file_ops_dir;
extern struct file_ops ext2_file_ops_file;
extern struct inode_ops ext2_inode_ops_symlink;

sector_t ext2_block_alloc(struct ext2_super_block *);
void ext2_block_release(struct ext2_super_block *, sector_t);

void ext2_inode_setup_ops(struct inode *inode);

/* Releases blocks associated with a paticular inode.
 *
 * Takes a size so it can be used to implement truncate. The ext2_inode is
 * modified to reflect the new size. Note that if called with a larger size
 * then the current size, this is a no-op, because the blocks will be allocated
 * on use.
 *
 * Must have a write-lock on the inode.
 */
int __ext2_inode_truncate(struct ext2_inode *, off_t size);
int ext2_truncate(struct inode *, off_t size);

sector_t ext2_bmap(struct inode *i, sector_t inode_sector);
sector_t ext2_bmap_alloc(struct inode *i, sector_t inode_sector);

__must_check struct block *__ext2_lookup_entry(struct inode *dir, const char *name, size_t len, struct ext2_disk_directory_entry **result);
__must_check struct block *__ext2_add_entry(struct inode *dir, const char *name, size_t len, struct ext2_disk_directory_entry **result, int *err);
int __ext2_dir_remove_entry(struct inode *dir, struct block *b, struct ext2_disk_directory_entry *entry);

int __ext2_dir_lookup_ino(struct inode *dir, const char *name, size_t len, ino_t *ino);
int __ext2_dir_lookup(struct inode *dir, const char *name, size_t len, struct inode **result);
int __ext2_dir_entry_exists(struct inode *dir, const char *name, size_t len);
int __ext2_dir_add(struct inode *dir, const char *name, size_t len, ino_t ino, mode_t mode);
int __ext2_dir_remove(struct inode *dir, const char *name, size_t len);
int __ext2_dir_readdir(struct file *filp, struct file_readdir_handler *handler);
int __ext2_dir_read_dent(struct file *filp, struct dent *dent, size_t size);
int __ext2_dir_empty(struct inode *dir);
int __ext2_dir_change_dotdot(struct inode *dir, ino_t ino);
int ext2_inode_new(struct super_block *sb, struct inode **result);

#endif
