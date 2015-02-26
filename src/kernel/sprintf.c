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

struct printf_backbone_str {
    struct printf_backbone backbone;
    char *buf;
    size_t left;
};

static void str_putchar(struct printf_backbone *b, char ch)
{
    struct printf_backbone_str *str = container_of(b, struct printf_backbone_str, backbone);
    if (str->left == 0)
        return ;

    *str->buf = ch;
    str->buf++;
    str->left--;
}

static void str_putstr(struct printf_backbone *b, const char *s)
{
    struct printf_backbone_str *str = container_of(b, struct printf_backbone_str, backbone);

    for (; *s && str->left; s++, str->buf++, str->left--)
        *str->buf = *s;
}

char *snprintfv(char *buf, size_t len, const char *fmt, va_list lst)
{
    struct printf_backbone_str str = {
        .backbone = {
            .putchar = str_putchar,
            .putstr  = str_putstr,
        },
        .buf = buf,
        .left = len - 1,
    };

    basic_printfv(&str.backbone, fmt, lst);

    *str.buf = '\0';

    return buf;
}


char *snprintf(char *buf, size_t len, const char *fmt, ...)
{
    char *ret;
    va_list lst;
    va_start(lst, fmt);
    ret = snprintfv(buf, len, fmt, lst);
    va_end(lst);
    return ret;
}

