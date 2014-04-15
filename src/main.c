/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <arch/types.h>
#include <term.h>

int cmain(uint32_t magic, uint32_t addr)
{
    int i;
    volatile int c;
    term_init();

    term_setcurcolor(term_make_color(T_WHITE, T_BLACK));
    for (i = 0; ; i++) {
        term_print("Tes");
        term_setcurcolor(term_make_color(i % 8, T_BLACK));
        for (c = 0; c < 100000; c++) {
            ;
        }
    }

    while (1);
}

