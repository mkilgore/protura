/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_FILE_SYSTEM_H
#define INCLUDE_FS_FILE_SYSTEM_H

#ifdef __KERNEL__

#include <protura/types.h>
#include <protura/list.h>

struct super_block;

struct file_system {
    const char *name;
    struct super_block *(*read_sb) (dev_t dev);

    list_node_t fs_list_entry;
};

void file_systems_init(void);

void file_system_register(struct file_system *);
void file_system_unregister(const char *name);

struct file_system *file_system_lookup(const char *name);

extern const struct file_ops file_system_file_ops;

#endif

#endif
