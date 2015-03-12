/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_DEBUG_H
#define INCLUDE_PROTURA_DEBUG_H

#include <protura/compiler.h>
#include <protura/stdarg.h>

void kprintf(const char *s, ...) __printf(1, 2);
void kprintfv(const char *s, va_list);

void panic(const char *s, ...) __printf(1, 2) __noreturn;
void panicv(const char *s, va_list) __noreturn;

#endif
