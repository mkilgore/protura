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
#include <config/autoconf.h>
#include <protura/spinlock.h>
#include <protura/basic_printf.h>
#include <drivers/term.h>
#include <mm/memlayout.h>

#include <arch/string.h>

#include <arch/asm.h>

#define TERM_MEMLOC ((void *)(0xB8000 + KMEM_KBASE))

struct term_info {
    struct spinlock lock;
    struct term_char (*buf)[TERM_COLS];

    uint8_t cur_r, cur_c;
    uint8_t cur_col;
};

static struct term_info glob_term;

static __always_inline void __term_updatecur(void)
{
    uint16_t curloc = glob_term.cur_r * 80 + glob_term.cur_c;
    outb(0x3D4, 14);
    outb(0x3D5, curloc >> 8);
    outb(0x3D4, 15);
    outb(0x3D5, curloc & 0xFF);
}

static void __term_clear_color(uint8_t color)
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

void term_clear_color(uint8_t color)
{
    using_spinlock(&glob_term.lock)
        __term_clear_color(color);
}

static void __term_clear(void)
{
    term_clear_color(glob_term.cur_col);
}

void term_clear(void)
{
    using_spinlock(&glob_term.lock)
        __term_clear();
}

void term_init(void)
{
    memset(&glob_term, 0, sizeof(struct term_info));

    glob_term.cur_col = 0x07;

    glob_term.buf = TERM_MEMLOC;
    __term_clear();
    __term_updatecur();
}

static void __term_setcurcolor(uint8_t color)
{
    glob_term.cur_col = color;
}

void term_setcurcolor(uint8_t color)
{
    using_spinlock(&glob_term.lock)
        __term_setcurcolor(color);
}

static void __term_setcur(int row, int col)
{
    glob_term.cur_r = row;
    glob_term.cur_c = col;
    __term_updatecur();
}

void term_setcur(int row, int col)
{
    using_spinlock(&glob_term.lock)
        __term_setcur(row, col);
}

static void __term_scroll(int lines)
{
    memmove(glob_term.buf,
            glob_term.buf[lines],
            TERM_COLS * sizeof(struct term_char) * (TERM_ROWS - lines));

    memset(glob_term.buf + TERM_ROWS - lines,
            0,
            TERM_COLS * sizeof(struct term_char) * lines);
}

void term_scroll(int lines)
{
    using_spinlock(&glob_term.lock)
        __term_scroll(lines);
}

static void __term_putchar_nocur(char ch)
{
    uint8_t r = glob_term.cur_r;
    uint8_t c = glob_term.cur_c;
    switch (ch) {
    case '\n':
        c = 0;
        r++;
        break;
    case '\t':
        c += (c % 8);
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
        __term_scroll(1);
        r--;
    }
    glob_term.cur_c = c;
    glob_term.cur_r = r;
}

void term_putchar(char ch)
{
    using_spinlock(&glob_term.lock) {
        __term_putchar_nocur(ch);
        __term_updatecur();
    }
}

void term_putstr(const char *s)
{

    using_spinlock(&glob_term.lock) {
        for (; *s; s++)
            __term_putchar_nocur(*s);

        __term_updatecur();
    }
}

void term_putnstr(const char *s, size_t len)
{
    using_spinlock(&glob_term.lock) {
        size_t l;
        for (l = 0; l < len; l++)
            __term_putchar_nocur(s[l]);

        __term_updatecur();
    }
}

static void term_printf_putchar(struct printf_backbone *b, char ch)
{
    term_putchar(ch);
}

static void term_printf_putnstr(struct printf_backbone *b, const char *s, size_t len)
{
    term_putnstr(s, len);
}

void term_printfv(const char *s, va_list lst)
{
    struct printf_backbone backbone = {
        .putchar = term_printf_putchar,
        .putnstr = term_printf_putnstr,
    };
    basic_printfv(&backbone, s, lst);
}

void term_printf(const char *s, ...)
{
    va_list lst;
    va_start(lst, s);
    term_printfv(s, lst);
    va_end(lst);
}

