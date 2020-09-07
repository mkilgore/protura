/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_CHAR_H
#define INCLUDE_FS_CHAR_H

#include <protura/fs/inode.h>
#include <protura/fs/file.h>

struct char_device {
    const char *name;
    int major;

    struct file_ops *fops;
};

enum {
    CHAR_DEV_NONE = 0,
    CHAR_DEV_TTY = 5,
    CHAR_DEV_MEM = 6,
    CHAR_DEV_SERIAL_TTY = 7,
    CHAR_DEV_QEMU_DBG = 8,
    CHAR_DEV_FB = 9,
    CHAR_DEV_EVENT = 10,
};

void char_dev_init(void);

struct char_device *char_dev_get(dev_t device);

extern struct file_ops char_dev_fops;

#endif
