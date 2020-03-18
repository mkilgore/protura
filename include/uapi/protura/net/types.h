/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef __INCLUDE_UAPI_PROTURA_NET_TYPES_H__
#define __INCLUDE_UAPI_PROTURA_NET_TYPES_H__

#include <protura/types.h>

/* Most of this stuff is mapped to macros because the matching kernel
 * header unmaps most of it for kernel-specific definitions */

#define __kn16 __kuint16_t
#define __kn32 __kuint32_t

static inline __kn32 ____khtonl(__kuint32_t hostl)
{
    return ((hostl & 0xFF000000) >> 24)
         | ((hostl & 0x00FF0000) >> 8)
         | ((hostl & 0x0000FF00) << 8)
         | ((hostl & 0x000000FF) << 24);
}

static inline __kn16 ____khtons(__kuint16_t hosts)
{
    return ((hosts & 0xFF00) >> 8)
         | ((hosts & 0x00FF) << 8);
}

#define __khtonl ____khtonl
#define __kthons ____khtons
#define __kntohl ____khtonl
#define __kntohs ____khtons

#define __kn32_make(n) (n)
#define __kn16_make(n) (n)

#define __kn32_to_uint32(n) (n)
#define __kn16_to_uint16(n) (n)

#endif
