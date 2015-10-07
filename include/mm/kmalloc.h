/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_MM_KMALLOC_H
#define INCLUDE_MM_KMALLOC_H

#include <protura/types.h>
#include <arch/pmalloc.h>
#include <protura/string.h>

void kmalloc_init(void);

void *kmalloc(ksize_t size, int flags);
ksize_t ksize(void *p);
void  kfree(void *p);

static inline void *kzalloc(ksize_t size, int flags)
{
    void *m = kmalloc(size, flags);
    if (m)
        memset(m, 0, size);
    return m;
}

#endif
