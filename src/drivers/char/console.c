/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/basic_printf.h>
#include <protura/scheduler.h>
#include <protura/wait.h>

#include <arch/spinlock.h>
#include <arch/drivers/keyboard.h>
#include <arch/asm.h>
#include <protura/fs/char.h>
#include <protura/drivers/vt.h>
#include <protura/drivers/screen.h>
#include <protura/drivers/keyboard.h>
#include <protura/drivers/tty.h>

#include "vt_internal.h"

static void vt_tty_init(struct tty *tty)
{
    struct vt *vt = container_of(tty->driver, struct vt, driver);
    vt->tty = tty;

    keyboard_set_tty(vt->tty);
}

static struct tty_ops vt_ops = {
    .init = vt_tty_init,
    .write = vt_tty_write,
};

static struct vt console_vt = {
    .driver = {
        .name = "tty",
        .minor_start = 1,
        .minor_end = 1,
        .ops = &vt_ops,
    },
    .screen = &arch_screen,
    .lock = SPINLOCK_INIT("console-vt-lock"),
};

static struct vt_kp_output vt_kp_output = {
    .output = KP_OUTPUT_INIT((vt_kp_output).output, vt_early_printf, "console-vt"),
    .vt = &console_vt,
};

void vt_console_kp_register(void)
{
    kp_output_register(&vt_kp_output.output);
}

void vt_console_kp_unregister(void)
{
    kp_output_unregister(&vt_kp_output.output);
}

void vt_console_early_init(void)
{
    vt_early_init(&console_vt);
}

void vt_console_init(void)
{
    vt_init(&console_vt);
}
