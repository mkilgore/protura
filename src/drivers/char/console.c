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
#include <protura/wait.h>

#include <arch/spinlock.h>
#include <arch/drivers/keyboard.h>
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

static int tty_keyboard_read(struct tty *driver, char *buf, size_t len)
{
    char c;
    size_t cur_pos = 0;

    while ((cur_pos != len && (c = arch_keyboard_has_char()))) {
        buf[cur_pos] = arch_keyboard_get_char();
        cur_pos++;
    }

    return cur_pos;
}

static int tty_keyboard_has_chars(struct tty *driver)
{
    return arch_keyboard_has_char();
}

static int tty_console_write(struct tty *driver, const char *data, size_t size)
{
    term_putnstr(data, size);

    return size;
}

static void tty_console_reigster_for_wakeups(struct tty *tty)
{
    arch_keyboard_work_add(&tty->work);
}

static struct tty_ops ops = {
    .read = tty_keyboard_read,
    .has_chars = tty_keyboard_has_chars,
    .write = tty_console_write,
    .register_for_wakeups = tty_console_reigster_for_wakeups,
};

static struct tty_driver driver = {
    .name = "tty",
    .minor_start = 1,
    .minor_end = 1,
    .ops = &ops,
};

void console_init(void)
{
    screen_init();
    keyboard_init();

    tty_driver_register(&driver);
}

