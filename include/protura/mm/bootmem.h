/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_MM_BOOTMEM_H
#define INCLUDE_MM_BOOTMEM_H

#include <protura/types.h>
#include <arch/spinlock.h>

/* Adds a memory region for use by the boot allocator */
int bootmem_add(pa_t start, pa_t end);

/* The regular bootmem_alloc() panics if the allocation cannot be done.
 * The _nopanic version returns NULL. */
void *bootmem_alloc(size_t length, size_t alignment);
void *bootmem_alloc_nopanic(size_t length, size_t alignment);

/* This call initializes palloc and passes the information from bootmem
 * over to palloc
 *
 * After this is called, bootmem_alloc() cannot be used!! */
void bootmem_setup_palloc(void);

#endif
