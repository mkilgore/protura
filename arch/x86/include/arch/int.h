/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_INT_H
#define INCLUDE_ARCH_INT_H

#include <arch/cpu.h>

#define in_atomic() \
    (cpu_get_local()->intr_count > 0)

#endif
