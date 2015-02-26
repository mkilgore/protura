/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>

#include <arch/paging.h>
#include <arch/kbrk.h>

static char *kern_start;
static char *kern_end;

void kbrk_init(void *kernel_start, void *kernel_end)
{
    kern_end = kernel_end;
    kern_start = kernel_start;
}

void *kbrk(size_t size, size_t align)
{
    void *obrk = ALIGN(kern_end, align);
    kern_end += size;
    return obrk;
}

void get_kernel_addrs(va_t *kernel_start, va_t *kernel_end)
{
    *kernel_start = (void *)kern_start;
    *kernel_end = (void *)kern_end;
}

