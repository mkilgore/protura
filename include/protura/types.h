/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_TYPES_H
#define INCLUDE_PROTURA_TYPES_H

#include <protura/config/autoconf.h>
#include <arch/types.h>
#include <protura/stddef.h>

#if 0
typedef __kuint8_t u8;
typedef __kuint16_t u16;
typedef __kuint32_t u32;
typedef __kuint64_t u64;

typedef __kint8_t s8;
typedef __kint16_t __ks16;
typedef __kint32_t __ks32;
typedef __kint64_t __ks64;
#endif

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

#ifdef __KERNEL__

#include <protura/compiler.h>

# define SECTOR_INVALID ((sector_t)-1)
# define UID_INVALID ((uid_t)-1)
# define GID_INVALID ((gid_t)-1)

typedef __koff_t off_t;
typedef __kpid_t pid_t;
typedef __kmode_t mode_t;
typedef __kdev_t dev_t;
typedef __ksector_t sector_t;
typedef __kino_t ino_t;
typedef __kumode_t umode_t;
typedef __ktime_t time_t;
typedef __kuseconds_t useconds_t;
typedef __ksuseconds_t suseconds_t;
typedef __kuid_t uid_t;
typedef __kgid_t gid_t;

typedef __kptrdiff_t ptrdiff_t;

static inline char __tolower(char c)
{
    if (c >= 'A' && c <= 'Z')
        c |= 0x20;
    return c;
}

static inline char __toupper(char c)
{
    if (c >= 'a' && c <= 'z')
        c &= ~0x20;
    return c;
}

#define tolower(c) __tolower((c))
#define toupper(c) __toupper((c))

#endif

#endif
