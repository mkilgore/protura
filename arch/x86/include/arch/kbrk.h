/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_KBRK_H
#define INCLUDE_ARCH_KBRK_H

#include <protura/types.h>

void kbrk_init(void *kernel_start, void *kernel_end);

void *kbrk(size_t size, size_t align);

void get_kernel_addrs(va_t *kernel_start, va_t *kernel_end);

#endif
