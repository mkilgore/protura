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
#include <config/autoconf.h>
#include <protura/skprintf.h>

#include <arch/debug.h>
#include <arch/backtrace.h>
#include <arch/asm.h>

void kprintfv(const char *fmt, va_list lst)
{
    arch_printfv(fmt, lst);
}

void kprintf(const char *fmt, ...)
{
    va_list lst;
    va_start(lst, fmt);
    kprintfv(fmt, lst);
    va_end(lst);
}

__noreturn void panicv(const char *fmt, va_list lst)
{
    kprintfv(fmt, lst);

    dump_stack();

    while (1)
        hlt();
}

__noreturn void panic(const char *fmt, ...)
{
    va_list lst;
    va_start(lst, fmt);
    panicv(fmt, lst);
    va_end(lst);
    while (1);
}

