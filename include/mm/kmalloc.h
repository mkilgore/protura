/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_MM_KMALLOC_H
#define INCLUDE_MM_KMALLOC_H

#define KMEM_ATOMIC 1
#define KMEM_DMA    2

void  kmalloc_init(void *start, void *end);
void *kmalloc(size_t size, int flags);
void  kfree(void *p);

#endif
