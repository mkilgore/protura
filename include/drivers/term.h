/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_TERM_H
#define INCLUDE_TERM_H

#include <stdarg.h>

#include <protura/types.h>
#include <protura/compiler.h>

#define TERM_ROWS 25
#define TERM_COLS 80

struct term_char {
    char chr;
    uint8_t color;
} __packed;

#define TERM_SIZE (TERM_ROWS * TERM_COLS * sizeof(struct term_char))

#define T_BLACK   0
#define T_BLUE    1
#define T_GREEN   2
#define T_CYAN    3
#define T_RED     4
#define T_MAGENTA 5
#define T_YELLOW  6
#define T_WHITE   7

/* combine this with any of the term_color's to get the 'bright' version */
#define T_BRIGHT 8

#define term_make_color(fg, bg) ((bg) << 4 | (fg))
#define term_bg(color) ((color) >> 4)
#define term_fg(color) ((color) & 0xF)

void term_init(void);

void term_put_term_char(struct term_char);

void term_putchar(char);
void term_putstr(const char *);
void term_putnstr(const char *, size_t len);

void term_printf(const char *, ...) __printf(1, 2);
void term_printfv(const char *, va_list);

void term_put_term_char_at(int row, int col, struct term_char);
void term_put_char_at(int row, int col, char);
void term_print_at(int row, int col, const char *);

void term_setcur(int r, int c);
void term_setcurcolor(uint8_t color);
void term_clear(void);
void term_clear_color(uint8_t color);

void term_scroll(int);

struct term_char term_getchar(int x, int y);
void term_getcurxy(int *x, int *y);
uint8_t term_getcurcol(void);

#endif

