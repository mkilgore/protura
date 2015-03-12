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

void kmalloc_init(void);

void *kmalloc(size_t size, int flags);
void *kzmalloc(size_t size, int flags);
size_t ksize(void *p);
void  kfree(void *p);

#endif
