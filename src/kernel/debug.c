/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/stdarg.h>
#include <protura/debug.h>
#include <drivers/term.h>

void kprintfv(const char *fmt, va_list lst)
{
    term_printfv(fmt, lst);
}

void kprintf(const char *fmt, ...)
{
    va_list lst;
    va_start(lst, fmt);
    kprintfv(fmt, lst);
    va_end(lst);
}

void panicv(const char *fmt, va_list lst)
{
    kprintfv(fmt, lst);
    while (1);
}

void panic(const char *fmt, ...)
{
    va_list lst;
    va_start(lst, fmt);
    kprintfv(fmt, lst);
    va_end(lst);

    while (1);
}

