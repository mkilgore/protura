/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_BASIC_PRINTF_H
#define INCLUDE_PROTURA_BASIC_PRINTF_H

#include <protura/types.h>
#include <protura/stdarg.h>
#include <protura/compiler.h>

struct printf_backbone {
    void (*putchar) (struct printf_backbone *, char ch);
    void (*putnstr) (struct printf_backbone *, const char *str, size_t len);
};

void basic_printf(struct printf_backbone *backbone, const char *s, ...) __printf(2, 3);
void basic_printfv(struct printf_backbone *backbone, const char *s, va_list args);

#endif
