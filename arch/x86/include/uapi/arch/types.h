/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef __INCLUDE_UAPI_ARCH_TYPES_H__
#define __INCLUDE_UAPI_ARCH_TYPES_H__

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

#endif

