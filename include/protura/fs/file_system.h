/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_FILE_SYSTEM_H
#define INCLUDE_FS_FILE_SYSTEM_H

#include <protura/types.h>
#include <protura/bits.h>
#include <protura/list.h>

struct super_block;

enum {
    FILE_SYSTEM_NODEV,
};

struct file_system {
    const char *name;
    flags_t flags;

    struct super_block *(*super_alloc) (void);
    void (*super_dealloc) (struct super_block *sb);

    int (*read_sb2) (struct super_block *);

    list_node_t fs_list_entry;
};

void file_system_register(struct file_system *);
void file_system_unregister(const char *name);

struct file_system *file_system_lookup(const char *name);

extern const struct file_ops file_system_file_ops;

#endif
