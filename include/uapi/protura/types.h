/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef __INCLUDE_UAPI_PROTURA_TYPES_H__
#define __INCLUDE_UAPI_PROTURA_TYPES_H__

#include <arch/types.h>

typedef long __koff_t;
typedef __kint32_t __kpid_t;

typedef __kuint32_t __kmode_t;

typedef __kuint32_t __kdev_t;
typedef __kuint16_t __kudev_t; /* Userspace dev_t */
typedef __kuint32_t __ksector_t;

typedef __kuint32_t __kino_t;
typedef __kuint16_t __kumode_t;

typedef long __ktime_t;
typedef __kint32_t __kuseconds_t;
typedef long     __ksuseconds_t;

typedef __kint32_t __kuid_t;
typedef __kint32_t __kgid_t;

typedef char * __kcaddr_t;
typedef __kuintptr_t __kdaddr_t;

typedef long __kptrdiff_t;

#endif
