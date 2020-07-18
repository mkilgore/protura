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
#include <protura/compiler.h>
#include <protura/mm/memlayout.h>

#include <arch/spinlock.h>
#include <arch/string.h>
#include <arch/drivers/screen.h>
#include <protura/drivers/screen.h>

#include <arch/asm.h>

#define SCR_MEMLOC ((void *)(0xB8000 + KMEM_KBASE))

static void scr_enable_cursor(struct screen *screen)
{
    /* Enable cursor from scanlines 13 to 15 */
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x0D);

    outb(0x3D4, 0x0B);
    outb(0x3D5, 0x0F);
}

static void scr_disable_cursor(struct screen *screen)
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x0D | 0x20);
}

static void scr_updatecur(struct screen *screen, int row, int col)
{
    uint16_t curloc = row * SCR_COLS + col;
    outb(0x3D4, 14);
    outb(0x3D5, curloc >> 8);
    outb(0x3D4, 15);
    outb(0x3D5, curloc & 0xFF);
}

struct screen arch_screen = {
    .buf = SCR_MEMLOC,
    .move_cursor = scr_updatecur,
    .cursor_on = scr_enable_cursor,
    .cursor_off = scr_disable_cursor,
};

void arch_screen_init(void)
{
    /* We don't use the argument so NULL is ok */
    scr_enable_cursor(NULL);
}
