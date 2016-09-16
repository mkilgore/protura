/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_TYPES_H
#define INCLUDE_ARCH_TYPES_H

typedef unsigned char __kuint8_t;
typedef signed char __kint8_t;

typedef unsigned short __kuint16_t;
typedef short __kint16_t;

typedef unsigned int __kuint32_t;
typedef int __kint32_t;

typedef unsigned long long __kuint64_t;
typedef long long __kint64_t;

typedef __kuint32_t __ksize_t;

typedef __kuint32_t __kuintptr_t;
typedef __kint32_t   __kintptr_t;

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
typedef void *    __kva_t;
typedef __kuintptr_t __kpa_t;

/* Page number */
typedef __kuint32_t __kpn_t;

/* I/O address */
typedef __kuint16_t __kio_t;

#ifdef __KERNEL__

# include <protura/compiler.h>

typedef __kuint8_t uint8_t;
typedef __kint8_t   int8_t;

typedef __kuint16_t uint16_t;
typedef __kint16_t   int16_t;

typedef __kuint32_t uint32_t;
typedef __kint32_t   int32_t;

typedef __kuint64_t uint64_t;
typedef __kint64_t   int64_t;

typedef __ksize_t size_t;

typedef __kuintptr_t uintptr_t;
typedef __kintptr_t   intptr_t;

typedef __kva_t va_t;
typedef __kpa_t pa_t;

typedef __kpn_t pn_t;
typedef __kio_t io_t;

# define va_make(va) ((va_t)(va))
# define pa_make(pa) ((pa_t)(pa))

# define va_val(va) ((void *)(va))
# define pa_val(pa) ((uintptr_t)(pa))

# define v_to_p(va) (pa_t)(V2P((uintptr_t)(va)))
# define p_to_v(pa) (va_t)(P2V((uintptr_t)(pa)))

#endif

#endif

