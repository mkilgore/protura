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
#include <protura/drivers/console.h>

#include "vt_internal.h"


/* This cannot be modified without holding the locks for all of the current
 * console_vts */
static int current_vt = 0;

static struct screen_char console_scr_bufs[CONSOLE_MAX][SCR_ROWS][SCR_COLS];
static struct screen console_screens[CONSOLE_MAX];
static struct vt console_vts[CONSOLE_MAX];

void console_switch_vt(int new_vt)
{
    /* To switch the current VT on the console, we first have to lock every VT.
     * We make sure to do this in order to prevent deadlocks. */
    int i;
    for (i = 0; i < CONSOLE_MAX; i++)
        spinlock_acquire(&console_vts[i].lock);

    /* Switching the active console is fairly simple. First we copy the current
     * screen state from `arch_screen` to the screen buffer for the current
     * screen */

    struct screen *real_screen = console_vts[current_vt].screen;
    if (!real_screen)
        real_screen = &arch_screen;

    memcpy(console_scr_bufs[current_vt],
           real_screen->buf,
           sizeof(**real_screen->buf) * SCR_COLS * SCR_ROWS);

    console_vts[current_vt].screen = console_screens + current_vt;

    current_vt = new_vt;

    memcpy(real_screen->buf,
           console_scr_bufs[current_vt],
           sizeof(**real_screen->buf) * SCR_COLS * SCR_ROWS);

    console_vts[current_vt].screen = real_screen;
    real_screen->move_cursor(real_screen, console_vts[current_vt].cur_row, console_vts[current_vt].cur_col);

    if (console_vts[current_vt].cursor_is_on)
        real_screen->cursor_on(real_screen);
    else
        real_screen->cursor_off(real_screen);

    if (real_screen->refresh)
        real_screen->refresh(real_screen);

    keyboard_set_tty(console_vts[current_vt].tty);

    for (i = CONSOLE_MAX - 1; i >= 0; i--)
        spinlock_release(&console_vts[i].lock);
}

void console_swap_active_screen(struct screen *new)
{
    int i;
    for (i = 0; i < CONSOLE_MAX; i++)
        spinlock_acquire(&console_vts[i].lock);

    console_vts[current_vt].screen = new;
    new->move_cursor(new, console_vts[current_vt].cur_row, console_vts[current_vt].cur_col);

    if (console_vts[current_vt].cursor_is_on)
        new->cursor_on(new);
    else
        new->cursor_off(new);

    if (new->refresh)
        new->refresh(new);

    for (i = CONSOLE_MAX - 1; i >= 0; i--)
        spinlock_release(&console_vts[i].lock);
}

static void vt_tty_init(struct tty *tty)
{
    struct vt *vt = container_of(tty->driver, struct vt, driver);
    vt->tty = tty;
}

static struct tty_ops vt_ops = {
    .init = vt_tty_init,
    .write = vt_tty_write,
};

static struct vt_kp_output vt_kp_output = {
    .output = KP_OUTPUT_INIT((vt_kp_output).output, vt_early_printf, "console-vt"),
    .vt = &console_vts[0],
};

void vt_console_kp_register(void)
{
    kp_output_register(&vt_kp_output.output);
}

void vt_console_kp_unregister(void)
{
    kp_output_unregister(&vt_kp_output.output);
}

static void console_screen_init(struct screen *screen, int num)
{
    screen->buf = console_scr_bufs[num];
}

static void console_init_struct(struct vt *vt, int num)
{
    vt->driver.major = CHAR_DEV_TTY;
    vt->driver.minor_start = num + 1;
    vt->driver.minor_end = num + 1;
    vt->driver.ops = &vt_ops;

    console_screen_init(console_screens + num, num);
    vt->screen = console_screens + num;
    spinlock_init(&vt->lock);
}

void vt_console_early_init(void)
{
    int i;
    for (i = 0; i < CONSOLE_MAX; i++)
        console_init_struct(console_vts + i, i);

    arch_screen_init();

    /* We only do the early init for the first VT */
    vt_early_init(console_vts);

    /* "Switch" to 0 to assign the arch_screen to it */
    console_swap_active_screen(&arch_screen);
}

void vt_console_init(void)
{
    int i;
    keyboard_init();

    for (i = 0; i < CONSOLE_MAX; i++)
        vt_init(console_vts + i);

    console_switch_vt(0);
}
