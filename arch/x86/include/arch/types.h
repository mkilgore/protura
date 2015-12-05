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

typedef uint32_t uintptr_t;
typedef int32_t   intptr_t;

/* These types are used to represent virtual and physical addresses. The fact
 * that one is a pointer and one is not means a warning is given if an
 * assignemt is done between the two without using a conversion function, like
 * v_to_p or V2P, which gives us some hope of catching errors of incorrectly
 * using one as the other.
 *
 * Note that GCC defines additions to void * types to be the same as though it
 * was a char *. Thus, it's legal to add and subtract from va_t types, even
 * though it's technically a void *. If this is a problem, it would be legal
 * to redefine va_t as 'uintptr_t', or 'char *' without any issues besides less
 * type guarentees. */
typedef void *    va_t;
typedef uintptr_t pa_t;

#define va_make(va) ((va_t)(va))
#define pa_make(pa) ((pa_t)(pa))

#define va_val(va) ((void *)(va))
#define pa_val(pa) ((uintptr_t)(pa))

#define v_to_p(va) (pa_t)(V2P((uintptr_t)(va)))
#define p_to_v(pa) (va_t)(P2V((uintptr_t)(pa)))

/* Page number */
typedef uint32_t pn_t;

/* I/O address */
typedef uint16_t io_t;

#endif

