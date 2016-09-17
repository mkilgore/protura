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

static int mem_zero_read(struct file *filp, void *vbuf, size_t len)
{
    memset(vbuf, 0, len);

    return len;
}

static int mem_zero_write(struct file *filp, const void *vbuf, size_t len)
{
    return len;
}

static off_t mem_zero_lseek(struct file *filp, off_t offset, int whence)
{
    return filp->offset = 0;
}

struct file_ops mem_zero_file_ops = {
    .open = NULL,
    .release = NULL,
    .read = mem_zero_read,
    .write = mem_zero_write,
    .lseek = mem_zero_lseek,
    .readdir = NULL,
};

