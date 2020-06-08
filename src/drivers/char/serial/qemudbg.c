/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/mm/user_check.h>
#include <arch/asm.h>

#include <protura/fs/char.h>
#include <protura/drivers/qemudbg.h>

static int qemu_dbg_open(struct inode *inode, struct file *filp)
{
    dev_t major = DEV_MAJOR(inode->dev_no);
    dev_t minor = DEV_MINOR(inode->dev_no);

    if (major != CHAR_DEV_QEMU_DBG || minor != 0)
        return -ENODEV;

    return 0;
}

static int qemu_dbg_read(struct file *filp, struct user_buffer vbuf, size_t len)
{
    return 0;
}

static int qemu_dbg_write(struct file *filp, struct user_buffer vbuf, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        char c;

        int ret = user_copy_to_kernel_indexed(&c, vbuf, i);
        if (ret)
            return ret;

        outb(QEMUDBG_PORT, c);
    }

    return len;
}

struct file_ops qemu_dbg_file_ops = {
    .open = qemu_dbg_open,
    .read = qemu_dbg_read,
    .write = qemu_dbg_write,
};
