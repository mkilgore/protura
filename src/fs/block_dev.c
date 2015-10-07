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
#include <mm/kmalloc.h>
#include <fs/block.h>
#include <protura/dev.h>

#include <drivers/ide.h>

static struct block_device devices[] = {
    [DEV_NONE] = {
        .name = "",
        .major = DEV_NONE,
        .block_size = 0,
        .ops = NULL
    },
    [DEV_IDE] = {
        .name = "ide",
        .major = DEV_IDE,
        .block_size = 512,
        .ops = &ide_block_device_ops
    }
};


void block_dev_init(void)
{
    ide_init();
}

struct block_device *block_dev_get(kdev_t device)
{
    int d = DEV_MAJOR(device);

    if (d < ARRAY_SIZE(devices))
        return devices + d;
    else
        return NULL;
}

