/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/stdarg.h>
#include <protura/types.h>
#include <protura/limits.h>
#include <protura/string.h>
#include <protura/debug.h>
#include <protura/basic_printf.h>
#include <arch/drivers/com1_debug.h>

//#define static

static char inttohex[][16] = { "0123456789abcdef", "0123456789ABCDEF" };

static void basic_printf_add_str(struct printf_backbone *backbone, const char *s, size_t len)
{
    size_t l;
    if (!s) {
        s = "(null)";
        len = strlen("(null)");
    }

    if (backbone->putnstr) {
        backbone->putnstr(backbone, s, len);
        return ;
    }

    for (l = 0; l < len; l++)
        backbone->putchar(backbone, s[l]);
}

static void escape_string(struct printf_backbone *backbone, const char *code, size_t len, va_list *args)
{
    const char *s;
    int max_width = -1;
    int width = -1, i;
    int *use_width = &width;
    size_t slen;

    for (i = 0; i < len; i++) {
        switch (code[i]) {
        case '0':
            if (*use_width != -1)
                *use_width *= 10;
            break;

        case '1' ... '9':
            if (*use_width == -1)
                *use_width = code[i] - '0';
            else
                *use_width = (*use_width * 10) + (code[i] - '0');
            break;

        case '*':
            *use_width = va_arg(*args, int);
            break;

        case '.':
            use_width = &max_width;
            break;

        default:
            goto after_value;
        }
    }
  after_value:

    s = va_arg(*args, const char *);
    slen = strlen(s);

    if (max_width != -1) {
        if (width > max_width)
            width = max_width;

        if (slen > max_width)
            slen = max_width;
    }

    basic_printf_add_str(backbone, s, slen);

    if (width != -1 && width > slen)
        while (slen++ < width)
            backbone->putchar(backbone, ' ');
}

static void escape_integer(struct printf_backbone *backbone, const char *code, size_t len, va_list *args)
{
    char buf[3 * sizeof(long long) + 2], *ebuf = buf + sizeof(buf) - 1;
    int digit;
    int orig, i;

    orig = va_arg(*args, int);
    i = orig;

    if (i == 0)
        *--ebuf = '0';

    while (i != 0) {
        digit = i % 10;
        if (digit < 0)
            digit = -digit;
        i = i / 10;
        *--ebuf = inttohex[0][digit];
    }

    if (orig < 0)
        *--ebuf = '-';

    basic_printf_add_str(backbone, ebuf, buf + sizeof(buf) - ebuf - 1);
}

static void escape_hex(struct printf_backbone *backbone, const char *code, size_t len, va_list *args)
{
    int caps = 0, ptr = 0;
    int bytes = -1, width = -1;
    int zero_pad = 0;
    uint8_t digit;
    uint64_t val;
    char buf[2 * sizeof(long long) + 2], *ebuf = buf + sizeof(buf) - 1;
    int i;

    i = 0;

    for (i = 0; i < len - 1; i++) {
        switch (code[i]) {
        case '0':
            if (width == -1)
                zero_pad = 1;
            else
                width *= 10;
            break;

        case '1' ... '9':
            if (width == -1)
                width = code[i] - '0';
            else
                width = (width * 10) + (code[i] - '0');
            break;
        default:
            goto after_value;
        }
    }
  after_value:

    if (i < len && code[i] == 'l') {
        i++;
        if (i < len && code[i] == 'l') {
            /* long long - 8 bytes */
            i++;
            bytes = sizeof(long long);
        }
    }

    switch (code[len - 1]) {

    /* The capital cases fall-through to the non-capital cases */
    case 'P':
        caps = 1;
    case 'p':
        ptr = 1;
        if (bytes == -1)
            bytes = 4; /* PROTURA_BITS / CHAR_BIT; */
        zero_pad = 1;
        break;

    case 'X':
        caps = 1;
    case 'x':
        if (bytes == -1)
            bytes = 4;
        break;

    default:
        /* If we got here, we have an invalid code */
        break;
    }

    if (bytes == 4)
        val = va_arg(*args, uint32_t);
    else
        val = va_arg(*args, uint64_t);

    if (width == -1)
        width = bytes * 2;

    for (i = 0; ((zero_pad)? i < width: 0) || val; i++) {
        digit = val % 16;
        val = val >> 4;
        *--ebuf = inttohex[caps][digit];
    }

    /* Pad to width with spaces, if we're not zero-padding */
    for (; i < width; i++)
        *--ebuf = ' ';

    if (ptr) {
        *--ebuf = 'x';
        *--ebuf = '0';
    }

    basic_printf_add_str(backbone, ebuf, buf + sizeof(buf) - ebuf - 1);
}

static void escape_char(struct printf_backbone *backbone, const char *code, size_t len, va_list *args)
{
    int ch = va_arg(*args, int);

    backbone->putchar(backbone, ch);
}

struct printf_escape {
    char ch;
    void (*write) (struct printf_backbone *, const char *code, size_t len, va_list *args);
};

static struct printf_escape escape_codes[] = {
    { 'x', escape_hex },
    { 'X', escape_hex },
    { 'p', escape_hex },
    { 'P', escape_hex },
    { 'd', escape_integer },
    { 's', escape_string },
    { 'c', escape_char },
    { '\0', NULL }
};

const char *handle_escape(struct printf_backbone *backbone, const char *s, va_list *args)
{
    const char *start = s;

    for (; *s; s++) {
        struct printf_escape *es;
        for (es = escape_codes; es->ch; es++) {
            if (es->ch == *s) {
                es->write(backbone, start, s - start + 1, args);
                return s;
            }
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
            basic_printf_add_str(backbone, last_c, s - last_c);

        s = handle_escape(backbone, s + 1, &args);
        last_c = s + 1;
    }

    if (s - last_c > 0)
        basic_printf_add_str(backbone, last_c, s - last_c);

    return ;
}

void basic_printf(struct printf_backbone *backbone, const char *s, ...)
{
    va_list lst;
    va_start(lst, s);
    basic_printf(backbone, s, lst);
    va_end(lst);
}


