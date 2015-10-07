/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_ALIGN_H
#define INCLUDE_ARCH_ALIGN_H

#include <protura/compiler.h>

#define CACHELINE_SIZE 64

#define __align_cacheline __align(CACHELINE_SIZE)

#endif
