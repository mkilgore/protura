/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/stdarg.h>
#include <protura/types.h>
#include <protura/string.h>
#include <protura/basic_printf.h>
#include <arch/drivers/com1_debug.h>

static char inttohex[][16] = { "0123456789abcdef", "0123456789ABCDEF" };


static void basic_printf_add_str(struct printf_backbone *backbone, const char *s, size_t len)
{
    size_t l;
    if (!s)
        s = "(null)";

    if (backbone->putnstr) {
        backbone->putnstr(backbone, s, len);
        return ;
    }

    for (l = 0; l < len; l++)
        backbone->putchar(backbone, s[l]);
}

static void basic_printf_putstr(struct printf_backbone *backbone, const char *s)
{
    basic_printf_add_str(backbone, s, strlen(s));
}

static void basic_printf_putint(struct printf_backbone *backbone, int i)
{
    char buf[3 * sizeof(i)] = { [0 ... 11] = 'A' }, *ebuf = buf + 3 * sizeof(i) + 1;
    int digit;
    int orig = i;

    if (i == 0)
        *--ebuf = '0';

    while (i > 0) {
        digit = i % 10;
        i = i / 10;
        *--ebuf = inttohex[0][digit];
    }

    if (orig < 0)
        *--ebuf = '-';

    basic_printf_add_str(backbone, ebuf, buf + sizeof(buf) - ebuf + 1);
}

static void basic_printf_putint64(struct printf_backbone *backbone, uint64_t i)
{
    char buf[3 * sizeof(i)] = { [0 ... 11] = 'A' }, *ebuf = buf + 3 * sizeof(i) + 1;
    int digit;
    int orig = i;

    if (i == 0)
        *--ebuf = '0';

    while (i != 0) {
        digit = i % 10;
        i = i / 10;
        *--ebuf = inttohex[0][digit];
    }

    if (orig < 0)
        *--ebuf = '-';

    basic_printf_add_str(backbone, ebuf, buf + sizeof(buf) - ebuf + 1);
}

static void basic_printf_putptr32(struct printf_backbone *backbone, uint32_t p, int caps, int ptr)
{
    uint32_t val = p;
    uint8_t digit;
    int i;
    char buf[11] = { [0 ... 10] = 'A' }, *ebuf = buf + 11 + 1;

    for (i = 0; i < 8; i++) {
        digit = val % 16;
        val = val >> 4;
        *--ebuf = inttohex[caps][digit];
    }

    if (ptr) {
        *--ebuf = 'x';
        *--ebuf = '0';
    }

    basic_printf_add_str(backbone, ebuf, buf + sizeof(buf) - ebuf + 1);
}

static void basic_printf_putptr64(struct printf_backbone *backbone, uint64_t p, int caps, int ptr)
{
    uint64_t val = p;
    uint8_t digit;
    int i;
    char buf[22] = { [0 ... 10] = 'A' }, *ebuf = buf + 22 + 1;

    for (i = 0; i < 16; i++) {
        digit = val % 16;
        val = val >> 4;
        *--ebuf = inttohex[caps][digit];
    }

    if (ptr) {
        *--ebuf = 'x';
        *--ebuf = '0';
    }

    basic_printf_add_str(backbone, ebuf, buf + sizeof(buf) - ebuf + 1);
}

#if PROTURA_BITS == 32
# define basic_printf_putptr(c, p, ca, ptr) basic_printf_putptr32(c, p, ca, ptr)
#else
# define basic_printf_putptr(c, p, ca, ptr) basic_printf_putptr64(c, p, ca, ptr)
#endif

static const char *handle_percent(struct printf_backbone *backbone, const char *s, va_list *args)
{
    int caps = 0, ptr = 0;
    enum {
        CLEAN,
        LONG,
        LONG_LONG
    } state = CLEAN;

    for (; *s; s++) {
        switch(*s) {
        case 'c':
            backbone->putchar(backbone, (char)va_arg(*args, int));
            return s;

        case 'd':
            if (state == LONG_LONG)
                basic_printf_putint64(backbone, va_arg(*args, uint64_t));
            else
                basic_printf_putint(backbone, va_arg(*args, int));
            return s;

        case 'P':
            caps = 1;
        case 'p':
            ptr = 1;
            goto hex_value;

        case 'X':
            caps = 1;
        case 'x':
            ptr = 0;
            goto hex_value;

        hex_value:
            if (state == LONG_LONG)
                basic_printf_putptr64(backbone, va_arg(*args, uint64_t), caps, ptr);
            else
                basic_printf_putptr(backbone, va_arg(*args, uintptr_t), caps, ptr);
            return s;

        case 's':
            basic_printf_putstr(backbone, va_arg(*args, const char *));
            return s;

        case 'l':
            switch(state) {
            case CLEAN:
                state = LONG;
                break;
            case LONG:
                state = LONG_LONG;
                break;
            default:
                break;
            }
            break;
        }

    }

    return --s;
}

void basic_printfv(struct printf_backbone *backbone, const char *s, va_list args)
{
    const char *last_c = s;
    for (; *s; s++) {
        if (*s != '%')
            continue ;

        if (s - last_c > 0)
            backbone->putnstr(backbone, last_c, s - last_c);

        s = handle_percent(backbone, s + 1, &args);
        last_c = s + 1;
    }

    if (s - last_c > 0)
        backbone->putnstr(backbone, last_c, s - last_c);

    return ;
}

void basic_printf(struct printf_backbone *backbone, const char *s, ...)
{
    va_list lst;
    va_start(lst, s);
    basic_printf(backbone, s, lst);
    va_end(lst);
}


