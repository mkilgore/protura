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

#define KMEM_ATOMIC 1
#define KMEM_DMA    2

void  kmalloc_add_free_mem(void *start, size_t length);

void *kmalloc_get_page(void);
void *kmalloc(size_t size, int flags);
void  kfree(void *p);

#endif
