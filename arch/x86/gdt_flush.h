/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_INTERNAL_ARCH_GDT_FLUSH_H
#define INCLUDE_INTERNAL_ARCH_GDT_FLUSH_H

#include <protura/types.h>

void gdt_flush(uint32_t);

#endif
