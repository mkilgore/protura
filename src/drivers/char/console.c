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
#include <arch/drivers/keyboard.h>
#include <protura/drivers/term.h>
#include <arch/asm.h>
#include <protura/fs/char.h>
#include <protura/drivers/screen.h>
#include <protura/drivers/keyboard.h>
#include <protura/drivers/tty.h>

struct file_ops console_file_ops = {
    .open = NULL,
    .release = NULL,
    .read = keyboard_file_read,
    .write = screen_file_write,
    .lseek = NULL,
    .readdir = NULL,
};

static int tty_keyboard_read(struct tty_driver *driver, char *buf, size_t len)
{
    char c;
    size_t cur_pos = 0;

    while ((cur_pos != len && (c = arch_keyboard_has_char()))) {
        buf[cur_pos] = arch_keyboard_get_char();
        cur_pos++;
    }

    return cur_pos;
}

static int tty_keyboard_has_chars(struct tty_driver *driver)
{
    return arch_keyboard_has_char();
}

static int tty_console_write(struct tty_driver *driver, const char *data, size_t size)
{
    term_putnstr(data, size);

    return size;
}

static struct tty_ops ops = {
    .read = tty_keyboard_read,
    .has_chars = tty_keyboard_has_chars,
    .write = tty_console_write,
};

static struct tty_driver driver = {
    .name = "console-tty",
    .minor_start = 0,
    .minor_end = 0,
    .ops = &ops,
    .inout_buf_lock = MUTEX_INIT(driver.inout_buf_lock, "console-tty-inout-buf-lock"),
    .in_wait_queue = WAIT_QUEUE_INIT(driver.in_wait_queue, "console-tty-in-wait-queue"),
    .tty_driver_node = LIST_NODE_INIT(driver.tty_driver_node),
};

void console_init(void)
{
    screen_init();
    keyboard_init();

    tty_register(&driver);
    kp(KP_TRACE, "driver kernel_task: %p\n", driver.kernel_task);
    arch_keyboard_wakeup_add(driver.kernel_task);
}

