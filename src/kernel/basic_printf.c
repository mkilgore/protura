/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/stdarg.h>
#include <protura/types.h>
#include <protura/basic_printf.h>

static char inttohex[][16] = { "0123456789abcdef", "0123456789ABCDEF" };

static void basic_printf_add_str(struct printf_backbone *backbone, const char *s)
{
    if (!s)
        s = "(null)";

    if (backbone->putstr) {
        backbone->putstr(backbone, s);
        return ;
    }

    do
        backbone->putchar(backbone, *s);
    while (*++s);
}

static void basic_printf_putint(struct printf_backbone *backbone, int i)
{
    char buf[3 * sizeof(i)], *ebuf = buf + 3 * sizeof(i);
    int digit;
    int orig = i;

    *--ebuf = '\0';

    if (i == 0)
        *--ebuf = '0';

    while (i > 0) {
        digit = i % 10;
        i = i / 10;
        *--ebuf = inttohex[0][digit];
    }

    if (orig < 0)
        *--ebuf = '-';

    basic_printf_add_str(backbone, ebuf);
}

static void basic_printf_putint64(struct printf_backbone *backbone, uint64_t i)
{
    char buf[3 * sizeof(i)], *ebuf = buf + 3 * sizeof(i);
    int digit;
    int orig = i;

    *--ebuf = '\0';

    if (i == 0)
        *--ebuf = '0';

    while (i > 0) {
        digit = i % 10;
        i = i / 10;
        *--ebuf = inttohex[0][digit];
    }

    if (orig < 0)
        *--ebuf = '-';

    basic_printf_add_str(backbone, ebuf);
}

static void basic_printf_putptr32(struct printf_backbone *backbone, uint32_t p, int caps)
{
    uint32_t val = p;
    uint8_t digit;
    int i;
    char buf[11], *ebuf = buf + 11;

    *--ebuf = '\0';

    for (i = 0; i < 8; i++) {
        digit = val % 16;
        val = val >> 4;
        *--ebuf = inttohex[caps][digit];
    }

    *--ebuf = 'x';
    *--ebuf = '0';
    basic_printf_add_str(backbone, ebuf);
}

static void basic_printf_putptr64(struct printf_backbone *backbone, uint64_t p, int caps)
{
    uint64_t val = p;
    uint8_t digit;
    int i;
    char buf[22], *ebuf = buf + 22;

    *--ebuf = '\0';

    for (i = 0; i < 16; i++) {
        digit = val % 16;
        val = val >> 4;
        *--ebuf = inttohex[caps][digit];
    }

    *--ebuf = 'x';
    *--ebuf = '0';
    basic_printf_add_str(backbone, ebuf);
}

#if PROTURA_BITS == 32
# define basic_printf_putptr(c, p, ca) basic_printf_putptr32(c, p, ca)
#else
# define basic_printf_putptr(c, p, ca) basic_printf_putptr64(c, p, ca)
#endif

static const char *handle_percent(struct printf_backbone *backbone, const char *s, va_list *args)
{
    int caps = 0;
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

        case 'X':
        case 'P':
            caps = 1;
            /* Fall through */
        case 'x':
        case 'p':
            if (state == LONG_LONG)
                basic_printf_putptr64(backbone, va_arg(*args, uint64_t), caps);
            else
                basic_printf_putptr(backbone, va_arg(*args, uintptr_t), caps);
            return s;

        case 's':
            basic_printf_add_str(backbone, va_arg(*args, const char *));
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
    for (; *s; s++) {
        if (*s != '%') {
            backbone->putchar(backbone, *s);
            continue ;
        }

        s = handle_percent(backbone, s + 1, &args);
    }

    return ;
}

void basic_printf(struct printf_backbone *backbone, const char *s, ...)
{
    va_list lst;
    va_start(lst, s);
    basic_printf(backbone, s, lst);
    va_end(lst);
}


