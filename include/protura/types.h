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

/*
 * Types used to denoate 'network-order' 16 and 32-bit integers.
 */
typedef __kuint32_t __kn32;
typedef __kuint16_t __kn16;

static inline __kn32 __khtonl(__kuint32_t hostl)
{
    return ((hostl & 0xFF000000) >> 24)
         | ((hostl & 0x00FF0000) >> 8)
         | ((hostl & 0x0000FF00) << 8)
         | ((hostl & 0x000000FF) << 24);
}

static inline __kn16 __khtons(__kuint16_t hosts)
{
    return ((hosts & 0xFF00) >> 8)
         | ((hosts & 0x00FF) << 8);
}

#define __kntohl(netl) __khtonl(netl)
#define __kntohs(nets) __khtons(nets)

#ifdef __KERNEL__

#include <protura/compiler.h>

# define SECTOR_INVALID ((sector_t)-1)

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
typedef __kn16 n16;
typedef __kn32 n32;
typedef __kuid_t uid_t;
typedef __kgid_t gid_t;

#define htonl(host) __khtonl(host)
#define htons(host) __khtons(host)

#define ntohl(net) __kntohl(net)
#define ntohs(net) __kntohs(net)

#define tolower(c) \
    ({ \
        typeof(c) ____ctmp = (c); \
        if (____ctmp >= 'A' && ____ctmp <= 'Z') \
            ____ctmp |= 0x20; \
        ____ctmp; \
    })

#define toupper(c) \
    ({ \
        typeof(c) ____ctmp = (c); \
        if (____ctmp >= 'a' && ____ctmp <= 'z') \
            ____ctmp &= ~0x20; \
        ____ctmp; \
    })

#endif

#endif
