/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_PAT_H
#define INCLUDE_ARCH_PAT_H

#include <protura/types.h>

enum pat_memory_type {
    PAT_MEM_UNCACHED,
    PAT_MEM_WRITE_COMBINED,
    PAT_MEM_WRITE_THROUGH = 4,
    PAT_MEM_WRITE_PROTECTED,
    PAT_MEM_WRITE_BACK,
    PAT_MEM_UNCACHED_WEAK,
};

#define MSR_PAT 0x00000277

#define PAT_ENT(n, t) \
    (((uint64_t)t) << (n * 8))

#endif
