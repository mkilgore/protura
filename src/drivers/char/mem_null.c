/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>

#include <protura/fs/char.h>
#include <protura/drivers/mem.h>

static int mem_null_read(struct file *filp, void *buf, size_t len)
{
    return 0;
}

static int mem_null_write(struct file *filp, const void *buf, size_t len)
{
    return len;
}

static off_t mem_null_lseek(struct file *filp, off_t offset, int whence)
{
    return filp->offset = 0;
}

struct file_ops mem_null_file_ops = {
    .open = NULL,
    .release = NULL,
    .read = mem_null_read,
    .write = mem_null_write,
    .lseek = mem_null_lseek,
    .readdir = NULL,
};

