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
#include <protura/spinlock.h>

#include <arch/debug.h>
#include <arch/backtrace.h>
#include <arch/asm.h>

static spinlock_t kprintf_lock = SPINLOCK_INIT(0);

void kprintfv_internal(const char *fmt, va_list lst)
{
    using_spinlock_nolog(&kprintf_lock)
        arch_printfv(fmt, lst);
}

void kprintf_internal(const char *fmt, ...)
{
    va_list lst;
    va_start(lst, fmt);
    kprintfv_internal(fmt, lst);
    va_end(lst);
}

