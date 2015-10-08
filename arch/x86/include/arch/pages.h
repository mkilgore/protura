/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_PAGES_H
#define INCLUDE_ARCH_PAGES_H

#include <protura/types.h>

/* Initalizes the physical memory manager by telling it which pages can be used */
void arch_pages_init(pa_t kernel_start, pa_t kernel_end, pa_t last_physical_addr);

#endif
