/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <drivers/term.h>

int kmain(void)
{
    term_setcurcolor(term_make_color(T_WHITE, T_BLUE));
    kprintf("\nKernel booted!\n");

    while (1);

    return 0;
}

