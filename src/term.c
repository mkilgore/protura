/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <arch/types.h>
#include <protura/string.h>
#include <term.h>

#define TERM_MEMLOC ((void *)0xB8000)

struct term_info {
    struct term_char (*buf)[TERM_COLS];

    uint8_t cur_r, cur_c;
    uint8_t cur_col;
};

static struct term_info glob_term;

void term_init(void)
{
    memset(&glob_term, 0, sizeof(struct term_info));

    glob_term.buf = TERM_MEMLOC;
    memset(glob_term.buf, 0, TERM_SIZE);
}

void term_setcurcolor(uint8_t color)
{
    glob_term.cur_col = color;
}

void term_scroll(int lines)
{
  //  memmove(glob_term.buf, glob_term.buf[lines], TERM_COLS * sizeof(struct term_char) * (1));
    memmove(glob_term.buf,
            glob_term.buf[lines],
            TERM_COLS * sizeof(struct term_char) * (TERM_ROWS - lines));

    memset(glob_term.buf + TERM_ROWS - lines,
            0,
            TERM_COLS * sizeof(struct term_char) * lines);
}

void term_putchar(const char ch)
{
    uint8_t r = glob_term.cur_r;
    uint8_t c = glob_term.cur_c;
    switch (ch) {
    case '\n':
        c = 0;
        r++;
        break;
    default:
        glob_term.buf[r][c].chr = ch;
        glob_term.buf[r][c].color = glob_term.cur_col;
        c++;
        if (c >= TERM_COLS) {
            c = 0;
            r++;
        }
        break;
    }
    if (r >= TERM_ROWS) {
        term_scroll(1);
        r--;
    }
    glob_term.cur_c = c;
    glob_term.cur_r = r;
}

void term_print(const char *s)
{
    for (; *s; s++)
        term_putchar(*s);
}

