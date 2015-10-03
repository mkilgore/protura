/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_TYPES_H
#define INCLUDE_PROTURA_TYPES_H

#include <arch/types.h>
#include <protura/stddef.h>
#include <protura/compiler.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef long koff_t;
typedef uint32_t kpid_t;

typedef uint32_t kmode_t;

typedef uint32_t kdev_t;
typedef uint32_t ksector_t;

typedef uint32_t kino_t;
typedef uint16_t kumode_t;

#endif
