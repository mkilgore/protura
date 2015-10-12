/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/list.h>
#include <mm/kmalloc.h>

#include <arch/spinlock.h>
#include <fs/block.h>
#include <fs/file.h>
#include <fs/file_system.h>
#include <fs/simple_fs.h>

struct file_ops simple_fs_file_ops = {
    .read = fs_file_generic_read,
    .write = fs_file_generic_write,
    .lseek = fs_file_generic_lseek,
    .readdir = NULL,
};

