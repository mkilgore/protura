/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/errors.h>
#include <protura/debug.h>
#include <protura/list.h>
#include <protura/hlist.h>
#include <protura/string.h>
#include <arch/spinlock.h>
#include <protura/atomic.h>
#include <protura/mm/kmalloc.h>

#include <protura/fs/block.h>
#include <protura/fs/super.h>
#include <protura/fs/file.h>
#include <protura/fs/inode.h>
#include <protura/fs/dent.h>
#include <protura/fs/vfs.h>
#include <protura/fs/fs.h>

struct getdents_callback {
    struct file_readdir_handler handler;
    struct dent *buf;
    struct dent *last;
    size_t size;
};


int fs_getdents(struct file *filp, struct dent *buf, size_t dent_buf_size)
{

}

