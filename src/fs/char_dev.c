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
#include <protura/kassert.h>
#include <protura/wait.h>

#include <arch/spinlock.h>
#include <drivers/console.h>
#include <drivers/keyboard.h>
#include <drivers/com.h>
#include <fs/file.h>
#include <fs/fs.h>
#include <fs/char.h>

static struct char_device devices[] = {
    [CHAR_DEV_NONE] = {
        .name = "",
        .major = CHAR_DEV_NONE,
        .fops = NULL,
    },
    [CHAR_DEV_CONSOLE] = {
        .name = "console",
        .major = CHAR_DEV_CONSOLE,
        .fops = &console_file_ops,
    },
    [CHAR_DEV_KEYBOARD] = {
        .name = "keyboard",
        .major = CHAR_DEV_KEYBOARD,
        .fops = &keyboard_file_ops,
    },
    [CHAR_DEV_COM] = {
        .name = "com",
        .major = CHAR_DEV_COM,
        .fops = &com_file_ops,
    },
};

struct char_device *char_dev_get(dev_t device)
{
    int c = DEV_MAJOR(device);

    kp(KP_TRACE, "DEV MAJOR=%d\n", c);

    if (c < ARRAY_SIZE(devices))
        return devices + c;
    else
        return NULL;
}

void char_dev_init(void)
{
    console_init();
    keyboard_init();
    com_init();
}

