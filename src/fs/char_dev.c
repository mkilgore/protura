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
#include <protura/drivers/console.h>
#include <protura/drivers/keyboard.h>
#include <protura/drivers/screen.h>
#include <protura/drivers/com.h>
#include <protura/drivers/tty.h>
#include <protura/drivers/mem.h>
#include <protura/fs/file.h>
#include <protura/fs/fs.h>
#include <protura/fs/char.h>

static struct char_device devices[] = {
    [CHAR_DEV_NONE] = {
        .name = "",
        .major = CHAR_DEV_NONE,
        .fops = NULL,
    },
#ifdef CONFIG_CONSOLE_DRIVER
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
    [CHAR_DEV_SCREEN] = {
        .name = "screen",
        .major = CHAR_DEV_SCREEN,
        .fops = &screen_file_ops,
    },
#endif
#ifdef CONFIG_PC_COM_SERIAL_DRIVER
    [CHAR_DEV_COM] = {
        .name = "com",
        .major = CHAR_DEV_COM,
        .fops = &com_file_ops,
    },
#endif
#ifdef CONFIG_TTY_DRIVER
    [CHAR_DEV_TTY] = {
        .name = "tty",
        .major = CHAR_DEV_TTY,
        .fops = &tty_file_ops,
    },
#endif
    [CHAR_DEV_MEM] = {
        .name = "mem",
        .major = CHAR_DEV_MEM,
        .fops = &mem_file_ops,
    },
};

struct char_device *char_dev_get(dev_t device)
{
    unsigned int c = DEV_MAJOR(device);

    kp(KP_TRACE, "DEV MAJOR=%d\n", c);

    if (c < ARRAY_SIZE(devices))
        return devices + c;
    else
        return NULL;
}

void char_dev_init(void)
{
    com_init();
    tty_subsystem_init();
}

