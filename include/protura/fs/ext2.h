/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_EXT2_H
#define INCLUDE_FS_EXT2_H

#include <protura/types.h>

struct ext2_disk_sb {
    uint32_t inode_total;
    uint32_t block_total;
    uint32_t suser_blocks;
    uint32_t block_unused_total;
    uint32_t inode_unused_total;
    uint32_t sb_block_number;
    uint32_t block_size_shift;
    uint32_t fragment_size_shift;
    uint32_t blocks_per_block_group;
    uint32_t fragments_per_block_group;
    uint32_t inodes_per_block_group;
    time_t   last_mount_time;
    time_t   last_write_time;
    uint16_t mounts_since_fsck;
    uint16_t mounts_allowed_until_fsck;
    uint16_t ext2_magic;
    uint16_t fs_state;
    uint16_t error_action;
    uint16_t version_minor;
    time_t   last_fsck;
    time_t   time_allowed_until_fsck;
    uint32_t os_id;
    uint32_t version_major;
    uint16_t uid_reserved_blocks;
    uint16_t gid_reserved_blocks;

    /* Extended super_block fields */
    uint32_t first_inode;
    uint16_t inode_entry_size;
    uint16_t owning_block_group;
    uint32_t optional_features;
    uint32_t required_features;
    uint32_t read_only_features;
    char     fs_id[16];
    char     volume_name[16]; /* C-String */
    char     last_mount_path[64]; /* C-String */
    uint32_t compression_used;
    uint8_t  file_block_prealloc_count;
    uint8_t  dir_block_prealloc_count;
    uint16_t unused1;
    char     journal_id[16];
    uint32_t journal_inode;
    uint32_t journal_device;
    uint32_t orphan_inode_list;
    uint32_t unused2;

};

#define EXT2_FS_STATE_CLEAN 1
#define EXT2_FS_STATE_ERROR 2

#define EXT2_ERROR_ACTION_IGNORE 1
#define EXT2_ERROR_ACTION_REMOUNT 2
#define EXT2_ERROR_ACTION_PANIC 3

#define EXT2_OS_ID_LINUX 0
#define EXT2_OS_ID_HURD 1
#define EXT2_OS_ID_MASIX 2
#define EXT2_OS_ID_FREEBSD 3
#define EXT2_OS_ID_OTHER 4

#define EXT2_REQUIRED_FEATURE_DIR_TYPE 0x02

#define EXT2_RO_FEATURE_SPARSE_SB 0x01
#define EXT2_RO_FEATURE_64BIT_LEN 0x02

#define EXT2_MAGIC 0xEF53

struct ext2_disk_block_group {
    uint32_t block_nr_block_bitmap;
    uint32_t block_nr_inode_bitmap;
    uint32_t block_nr_inode_table;
    uint16_t block_unused_total;
    uint16_t inode_unused_total;
    uint16_t directory_count;
    uint16_t pad;
    uint8_t  reserved[12];
};

struct ext2_disk_inode {
    uint16_t mode;
    uint16_t uid;
    uint32_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks; /* Reocrded in number of 512 byte blocks */
    uint32_t flags;
    uint32_t osd1;
    uint32_t blk_ptrs[15];
    uint32_t generation;
    uint32_t file_acl;
    uint32_t dir_acl;
    uint32_t faddr;
    uint8_t  osd2[12];
};

#define EXT2_BAD_INO 1
#define EXT2_ROOT_INO 2
#define EXT2_ACL_IDX_INO 3
#define EXT2_ACL_DATA_INO 4
#define EXT2_BOOT_LOADER_INO 5
#define EXT2_UNDEL_DIR_INO 6
#define EXT2_DEFAULT_FIRST_INO 11

struct ext2_disk_directory_entry {
    uint32_t ino;
    uint16_t rec_len;
    uint8_t  name_len_and_type[2];
    char name[];
};

#ifdef __KERNEL__

#include <protura/fs/block.h>
#include <protura/fs/inode.h>
#include <protura/fs/super.h>

struct ext2_super_block {
    struct super_block sb;

    int block_size;
    int block_group_count;

    int sb_block_nr;

    struct ext2_disk_sb disksb;

    struct ext2_disk_block_group *groups;
};

struct ext2_inode {
    struct inode i;

    sector_t inode_group_blk_nr;
    int inode_group_blk_offset;

    uint32_t flags;
    union {
        uint32_t blk_ptrs[15];
        struct {
            uint32_t blk_ptrs_direct[12];
            uint32_t blk_ptrs_single[1];
            uint32_t blk_ptrs_double[1];
            uint32_t blk_ptrs_triple[1];
        };
    };
};

void ext2_init(void);

#endif

#endif
