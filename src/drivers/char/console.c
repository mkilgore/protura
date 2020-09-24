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
#include <protura/kparam.h>
#include <protura/mm/kmalloc.h>
#include <protura/initcall.h>

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

static struct screen_char *console_scr_bufs[CONSOLE_MAX];

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

    memcpy(console_scr_bufs[current_vt],
           real_screen->buf,
           sizeof(*real_screen->buf) * real_screen->rows * real_screen->cols);

    console_vts[current_vt].screen = console_screens + current_vt;

    current_vt = new_vt;

    memcpy(real_screen->buf,
           console_scr_bufs[current_vt],
           sizeof(*real_screen->buf) * real_screen->rows * real_screen->cols);

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

static void console_swap_active_screen_with_bufs(struct screen *new, struct screen_char **new_bufs)
{
    int i;
    for (i = 0; i < CONSOLE_MAX; i++)
        spinlock_acquire(&console_vts[i].lock);

    for (i = 0; i < CONSOLE_MAX; i++) {
        struct screen_char *tmp = console_scr_bufs[i];

        /* FIXME: We should be duplicating the old data from the console onto
         * the new screen buffers, but unfortunately the "real" screen may be
         * gone at this point so we can't actually read data from it.
         *
         * Fixing this involves largely redoing the vt driver layer not
         * manipulate the screen buffer directly, but offer `putc()`,
         * `scroll()`, etc. functions to hook into, as well as keeping a copy
         * of the screen on a backing buffer. */

        console_scr_bufs[i] = new_bufs[i];
        console_screens[i].buf = new_bufs[i];
        console_screens[i].rows = new->rows;
        console_screens[i].cols = new->cols;
        new_bufs[i] = tmp;

        /* A bit of a hack, but we reset the scroll region to take advantage of the larger size */
        console_vts[i].scroll_bottom = new->rows;
    }

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

void console_swap_active_screen(struct screen *new)
{
    struct screen_char *new_bufs[CONSOLE_MAX];

    struct winsize new_size = {
        .ws_row = new->rows,
        .ws_col = new->cols,
    };

    /* We preemptively allocate new screen buffers as we can't allocate them
     * while holding all the spinlocks.
     *
     * Then, after swapping the screen, if necessary we free the unused or
     * existing bufs.*/

    int i;
    size_t size = sizeof(*new_bufs) * new->rows * new->cols;

    for (i = 0; i < CONSOLE_MAX; i++)
        new_bufs[i] = kzalloc(size, PAL_KERNEL);

    console_swap_active_screen_with_bufs(new, new_bufs);

    for (i = 0; i < CONSOLE_MAX; i++)
        tty_resize(console_vts[i].tty, &new_size);

    for (i = 0; i < CONSOLE_MAX; i++)
        if (new_bufs[i] != arch_static_console_scr_bufs[i])
            kfree(new_bufs[i]);
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

static struct kp_output_ops vt_kp_output_ops = {
    .print = vt_early_print,
};

static struct vt_kp_output vt_kp_output = {
    .output = KP_OUTPUT_INIT((vt_kp_output).output, KP_NORMAL, "console-vt", &vt_kp_output_ops),
    .vt = &console_vts[0],
};

KPARAM("console.loglevel", &vt_kp_output.output.max_level, KPARAM_LOGLEVEL);

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

    struct screen_char *bufs[CONSOLE_MAX];
    for (i = 0; i < CONSOLE_MAX; i++)
        bufs[i] = arch_static_console_scr_bufs[i];

    /* "Switch" to 0 to assign the arch_screen to it */
    console_swap_active_screen_with_bufs(&arch_screen, bufs);
}

static void vt_console_init(void)
{
    int i;
    for (i = 0; i < CONSOLE_MAX; i++)
        vt_init(console_vts + i);

    console_switch_vt(0);
}
initcall_subsys(vt_console, vt_console_init);
