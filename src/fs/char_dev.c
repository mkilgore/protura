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
#include <protura/scheduler.h>
#include <protura/dev.h>
#include <protura/wait.h>

#include <arch/spinlock.h>
#include <protura/drivers/console.h>
#include <protura/drivers/keyboard.h>
#include <protura/drivers/com.h>
#include <protura/drivers/tty.h>
#include <protura/drivers/mem.h>
#include <protura/drivers/qemudbg.h>
#include <protura/event/protocol.h>
#include <protura/video/fbcon.h>
#include <protura/fs/file.h>
#include <protura/fs/fs.h>
#include <protura/fs/char.h>

static struct char_device devices[] = {
    [CHAR_DEV_NONE] = {
        .name = "",
        .major = CHAR_DEV_NONE,
        .fops = NULL,
    },
#ifdef CONFIG_TTY_DRIVER
    [CHAR_DEV_TTY] = {
        .name = "tty",
        .major = CHAR_DEV_TTY,
        .fops = &tty_file_ops,
    },
    [CHAR_DEV_SERIAL_TTY] = {
        .name = "ttyS",
        .major = CHAR_DEV_SERIAL_TTY,
        .fops = &tty_file_ops,
    },
#endif
    [CHAR_DEV_MEM] = {
        .name = "mem",
        .major = CHAR_DEV_MEM,
        .fops = &mem_file_ops,
    },
#ifdef CONFIG_DRIVER_QEMU_DBG
    [CHAR_DEV_QEMU_DBG] = {
        .name = "qemudbg",
        .major = CHAR_DEV_QEMU_DBG,
        .fops = &qemu_dbg_file_ops,
    },
#endif
    [CHAR_DEV_FB] = {
        .name = "fb",
        .major = CHAR_DEV_FB,
        .fops = &fb_file_ops,
    },
    [CHAR_DEV_EVENT] = {
        .name = "event",
        .major = CHAR_DEV_EVENT,
        .fops = &event_file_ops,
    }
};

struct char_device *char_dev_get(dev_t device)
{
    unsigned int c = DEV_MAJOR(device);

    kp(KP_TRACE, "DEV MAJOR=%d\n", c);

    if (c < ARRAY_SIZE(devices) && devices[c].fops)
        return devices + c;
    else
        return NULL;
}

static int char_dev_open(struct inode *inode, struct file *filp)
{
    if (!inode->cdev || !inode->cdev->fops)
        return -ENODEV;

    filp->ops = inode->cdev->fops;

    return (filp->ops->open) (inode, filp);
}

struct file_ops char_dev_fops = {
    .open = char_dev_open,
};

void char_dev_init(void)
{
    com_init();
    tty_subsystem_init();
}

