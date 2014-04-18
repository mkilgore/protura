/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/string.h>
#include <protura/stdarg.h>
#include <protura/compiler.h>
#include <drivers/term.h>

#include <arch/asm.h>

#define TERM_MEMLOC ((void *)0xB8000)

struct term_info {
    struct term_char (*buf)[TERM_COLS];

    uint8_t cur_r, cur_c;
    uint8_t cur_col;
};

static struct term_info glob_term;

static __always_inline void term_updatecur(void)
{
    uint16_t curloc = glob_term.cur_r * 80 + glob_term.cur_c;
    outb(0x3D4, 14);
    outb(0x3D5, curloc >> 8);
    outb(0x3D4, 15);
    outb(0x3D5, curloc & 0xFF);
}

void term_clear_color(uint8_t color)
{
    int r, c;
    struct term_char tmp;
    if (color == 0) {
        memset(glob_term.buf, 0, TERM_SIZE);
        return ;
    }

    tmp.chr = 0;
    tmp.color = color;

    for (r = 0; r < TERM_ROWS; r++)
        for (c = 0; c < TERM_COLS; c++)
            glob_term.buf[r][c] = tmp;

    return ;
}

void term_clear(void)
{
    term_clear_color(glob_term.cur_col);
}

void term_init(void)
{
    memset(&glob_term, 0, sizeof(struct term_info));

    glob_term.cur_col = 0x07;

    glob_term.buf = TERM_MEMLOC;
    term_clear();
    term_updatecur();
}

void term_setcurcolor(uint8_t color)
{
    glob_term.cur_col = color;
}

void term_setcur(int row, int col)
{
    glob_term.cur_r = row;
    glob_term.cur_c = col;
    term_updatecur();
}

void term_scroll(int lines)
{
    memmove(glob_term.buf,
            glob_term.buf[lines],
            TERM_COLS * sizeof(struct term_char) * (TERM_ROWS - lines));

    memset(glob_term.buf + TERM_ROWS - lines,
            0,
            TERM_COLS * sizeof(struct term_char) * lines);
}

void term_putchar(char ch)
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

static void term_putstr(const char *s)
{
    for (; *s; s++)
        term_putchar(*s);

    term_updatecur();
}

static char inttohex[] = { "0123456789ABCDEF" };

static void term_putint(int i)
{
    char buf[3 * sizeof(i)], *ebuf = buf + 3 * sizeof(i);
    int digit;

    *--ebuf = '\0';

    if (i == 0)
        *--ebuf = '0';

    while (i > 0) {
        digit = i % 10;
        i = i / 10;
        *--ebuf = inttohex[digit];
    }
    term_putstr(ebuf);
}

#if BITS == 32
void term_putptr(const void *p)
{
    uint32_t val = (uint32_t)p;
    uint8_t digit;
    int i;
    char buf[11], *ebuf = buf + 11;

    *--ebuf = '\0';

    for (i = 0; i < 8; i++) {
        digit = val % 16;
        val = val >> 4;
        *--ebuf = inttohex[digit];
    }

    *--ebuf = 'x';
    *--ebuf = '0';
    term_putstr(ebuf);
}
#endif

void term_printfv(const char *s, va_list args)
{
    for (; *s; s++) {
        if (*s != '%') {
            term_putchar(*s);
            continue ;
        }

        s++;
        if (*s == '\0')
            break ;

        switch(*s) {
        case 'c':
            term_putchar((char)va_arg(args, int));
            break;
        case 'd':
            term_putint(va_arg(args, int));
            break;
        case 'p':
            term_putptr(va_arg(args, void *));
            break;
        case 's':
            term_putstr(va_arg(args, const char *));
            break;
        }
    }

    term_updatecur();

    return ;
}

void term_printf(const char *s, ...)
{
    va_list lst;
    va_start(lst, s);
    term_printfv(s, lst);
    va_end(lst);
}

