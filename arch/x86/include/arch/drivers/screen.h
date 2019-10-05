/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef ARCH_DRIVERS_SCREEN_H
#define ARCH_DRIVERS_SCREEN_H

#include <protura/types.h>
#include <protura/compiler.h>

struct screen_char {
    char chr;
    uint8_t color;
} __packed;

#define SCR_ROWS 25
#define SCR_COLS 80

#define SCR_SIZE (SCR_ROWS * SCR_COLS * sizeof(struct screen_char))

#define SCR_BLACK   0
#define SCR_BLUE    1
#define SCR_GREEN   2
#define SCR_CYAN    3
#define SCR_RED     4
#define SCR_MAGENTA 5
#define SCR_YELLOW  6
#define SCR_WHITE   7

/* combine this with any of the SCR_*'s to get the 'bright' version */
#define SCR_BRIGHT 8

#define screen_make_color(fg, bg) ((bg) << 4 | (fg))
#define screen_bg(color) ((color) >> 4)
#define screen_fg(color) ((color) & 0xF)

void arch_screen_init(void);

extern struct screen arch_screen;

#endif
