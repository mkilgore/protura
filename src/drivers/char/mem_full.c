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

static int mem_full_read(struct file *filp, void *buf, size_t len)
{
    memset(buf, 0, len);
    return len;
}

static int mem_full_write(struct file *flip, const void *buf, size_t len)
{
    return -ENOSPC;
}

static off_t mem_full_lseek(struct file *filp, off_t offset, int whence)
{
    switch (whence) {
    case SEEK_SET:
        return filp->offset = offset;

    case SEEK_CUR:
        filp->offset += offset;

        if (filp->offset < 0)
            filp->offset = 0;

        return filp->offset;

    default:
        return -EINVAL;
    }
}

struct file_ops mem_full_file_ops = {
    .open = NULL,
    .release = NULL,
    .read = mem_full_read,
    .write = mem_full_write,
    .lseek = mem_full_lseek,
    .readdir = NULL,
};

