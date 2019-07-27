/*
 * Copyright (C) 2018 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_DRIVERS_PL011_H
#define INCLUDE_DRIVERS_PL011_H

#include <protura/types.h>
#include <protura/stdarg.h>
#include <protura/errors.h>

#ifdef CONFIG_PL011_DRIVER
void pl011_init(void);
void pl011_tty_init(void);

int pl011_init_early(void);

void pl011_printfv(const char *fmt, va_list lst);
#else
static inline void pl011_init(void) { }
static inline void pl011_tty_init(void) { }

static inline int pl011_init_early(void)
{
    return -ENOTSUP;
}

static inline void pl011_printfv(const char *fmt, va_list lst) { }
#endif

#endif
