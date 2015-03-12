/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_ALLOC_H
#define INCLUDE_PROTURA_ALLOC_H

struct allocator {
    uintptr_t addr_start;
    uint32_t pages;
};

#endif
