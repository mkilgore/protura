/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_TYPES_H
#define INCLUDE_ARCH_TYPES_H

#include <protura/compiler.h>

typedef unsigned char uint8_t;
typedef signed char int8_t;

typedef unsigned short uint16_t;
typedef short int16_t;

typedef unsigned int uint32_t;
typedef int int32_t;

typedef unsigned long long uint64_t;
typedef long long int64_t;

typedef uint32_t size_t;
typedef int32_t ssize_t;

typedef uint32_t uintptr_t;
typedef int32_t   intptr_t;

/* General purpose int size */
typedef unsigned int pint __attribute__((__mode__(__word__)));

/*
struct virtual_addr { void *a; } __packed;
typedef struct virtual_addr va_t;

struct physical_addr { uintptr_t p; } __packed;
typedef struct physical_addr pa_t; */

typedef void *    va_t;
typedef uintptr_t pa_t;

#define va_make(va) ((va_t)(va))
#define pa_make(pa) ((pa_t)(pa))

#define va_val(va) ((void *)(va))
#define pa_val(pa) ((uintptr_t)(pa))

#define v_to_p(va) (pa_t)(V2P((uintptr_t)(va)))
#define p_to_v(pa) (va_t)(P2V((uintptr_t)(pa)))

#endif

