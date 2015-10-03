/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_SNPRINTF_H
#define INCLUDE_PROTURA_SNPRINTF_H

#include <protura/types.h>
#include <protura/stdarg.h>

ksize_t snprintf(char *buf, ksize_t len, const char *fmt, ...) __printf(3, 4);
ksize_t snprintfv(char *buf, ksize_t len, const char *fmt, va_list lst);

ksize_t sprintfv(char *buf, const char *fmt, va_list lst);
ksize_t sprintf(char *buf, const char *fmt, ...) __printf(2, 3);

#endif
