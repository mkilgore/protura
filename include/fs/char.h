/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_CHAR_H
#define INCLUDE_FS_CHAR_H

#include <fs/inode.h>
#include <fs/file.h>

struct char_device {
    const char *name;
    int major;

    struct file_ops *fops;
};

enum {
    CHAR_DEV_NONE = 0,
};

void char_dev_init(void);

int char_dev_file_open_generic(struct inode *dev, struct file *);
int char_dev_file_close_generic(struct inode *dev, struct file *);

struct char_device *char_dev_get(dev_t device);

#endif
