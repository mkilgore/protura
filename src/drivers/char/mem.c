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
#include <protura/dev.h>

#include <protura/event/device.h>
#include <protura/fs/char.h>
#include <protura/drivers/mem.h>

static int mem_open(struct inode *inode, struct file *filp)
{
    dev_t minor = DEV_MINOR(inode->dev_no);
    int ret = 0;

    switch (minor) {
    case MEM_MINOR_ZERO:
        filp->ops = &mem_zero_file_ops;
        break;

    case MEM_MINOR_FULL:
        filp->ops = &mem_full_file_ops;
        break;

    case MEM_MINOR_NULL:
        filp->ops = &mem_null_file_ops;
        break;

    default:
        ret = -ENODEV;
        break;
    }

    return ret;
}

struct file_ops mem_file_ops = {
    .open = mem_open,
    .release = NULL,
    .read = NULL,
    .write = NULL,
    .lseek = NULL,
    .readdir = NULL,
};

void mem_init(void)
{
    device_submit_char(KERN_EVENT_DEVICE_ADD, DEV_MAKE(CHAR_DEV_MEM, MEM_MINOR_ZERO));
    device_submit_char(KERN_EVENT_DEVICE_ADD, DEV_MAKE(CHAR_DEV_MEM, MEM_MINOR_NULL));
    device_submit_char(KERN_EVENT_DEVICE_ADD, DEV_MAKE(CHAR_DEV_MEM, MEM_MINOR_FULL));
}
