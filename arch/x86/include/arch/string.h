/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_STRING_H
#define INCLUDE_ARCH_STRING_H

#include <protura/types.h>

#define _STRING_ARCH_MEMSET
void memset(void *ptr, int c, size_t len);

#define _STRING_ARCH_MEMCPY
void memcpy(void *restrict dest, const void *restrict src, size_t len);


#define _STRING_ARCH_MEMMOVE
void memmove(void *dest, const void *src, size_t len);

#endif
