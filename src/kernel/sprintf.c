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

static void str_putnstr(struct printf_backbone *b, const char *s, size_t len)
{
    struct printf_backbone_str *str = container_of(b, struct printf_backbone_str, backbone);
    size_t l;

    for (l = 0; l < len; l++)
        str->buf[len - l] = s[l];
    str->buf += len;
}

size_t snprintfv(char *buf, size_t len, const char *fmt, va_list lst)
{
    struct printf_backbone_str str = {
        .backbone = {
            .putchar = str_putchar,
            .putnstr  = str_putnstr,
        },
        .buf = buf,
        .left = len - 1,
    };

    basic_printfv(&str.backbone, fmt, lst);

    *str.buf = '\0';

    return len - str.left;
}


size_t snprintf(char *buf, size_t len, const char *fmt, ...)
{
    size_t ret;
    va_list lst;
    va_start(lst, fmt);
    ret = snprintfv(buf, len, fmt, lst);
    va_end(lst);
    return ret;
}

