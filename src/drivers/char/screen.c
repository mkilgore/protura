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
#include <protura/drivers/term.h>
#include <arch/asm.h>
#include <protura/fs/char.h>
#include <protura/drivers/screen.h>

void screen_init(void)
{
    return ;
}

int screen_file_write(struct file *filp, const void *buf, size_t len)
{
    term_putnstr(buf, len);

    return len;
}

struct file_ops screen_file_ops = {
    .open = NULL,
    .release = NULL,
    .read = NULL,
    .write = screen_file_write,
    .lseek = NULL,
    .readdir = NULL,
};

