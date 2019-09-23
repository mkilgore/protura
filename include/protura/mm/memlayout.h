/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_MM_MEMLAYOUT_H
#define INCLUDE_MM_MEMLAYOUT_H

#include <arch/memlayout.h>

#define KTEST_SECTION \
    . = ALIGN(8); \
    __ktest_start = .; \
    KEEP(*(.ktest)); \
    __ktest_end = .;

#endif
