/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_DRIVERS_SCREEN_H
#define INCLUDE_DRIVERS_SCREEN_H

#include <protura/types.h>
#include <arch/drivers/screen.h>

struct screen {
    struct screen_char (*buf)[SCR_COLS];

    void (*move_cursor) (struct screen *, int row, int col);
    void (*cursor_on) (struct screen *);
    void (*cursor_off) (struct screen *);
    void (*refresh) (struct screen *);
};

#endif
