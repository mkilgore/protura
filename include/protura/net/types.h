/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_NET_TYPES_H
#define INCLUDE_PROTURA_NET_TYPES_H

#include <uapi/protura/net/types.h>

/* Get rid of the userspace definitions, as we have special matching kernel
 * definitions instead */

#undef __kn32
#undef __kn32_make
#undef __kn32_to_uint32
#undef __khtonl
#undef __kntohl

#undef __kn16
#undef __kn16_make
#undef __kn16_to_uint16
#undef __khtons
#undef __kntohs

/* 
 * We define "network-order" types as structures, which forces type errors if
 * we attempt to assign them directly from host types without doing a conversion.
 *
 * The downside is that this is incompatible with the userspace interface. To
 * allow these headers to be used, we map the network-order types directly to
 * regular integers and provide separate implementations of this interface,
 * allowing it to remain compatibile.
 */

typedef struct n16_struct {
    uint16_t v;
} __kn16;

typedef struct n32_struct {
    uint32_t v;
} __kn32;

typedef __kn16 n16;
typedef __kn32 n32;

static inline n32 __khtonl(uint32_t hostl)
{
    return (n32) {
        .v = ((hostl & 0xFF000000) >> 24)
             | ((hostl & 0x00FF0000) >> 8)
             | ((hostl & 0x0000FF00) << 8)
             | ((hostl & 0x000000FF) << 24)
    };
}

static inline n16 __khtons(uint16_t hosts)
{
    return (n16) {
        .v = ((hosts & 0xFF00) >> 8)
             | ((hosts & 0x00FF) << 8)
    };
}

static inline uint32_t __kntohl(n32 netl)
{
    uint32_t n = netl.v;

    return ((n & 0xFF000000) >> 24)
         | ((n & 0x00FF0000) >> 8)
         | ((n & 0x0000FF00) << 8)
         | ((n & 0x000000FF) << 24);
}

static inline uint16_t __kntohs(n16 netl)
{
    uint32_t n = netl.v;

    return ((n & 0xFF00) >> 8)
         | ((n & 0x00FF) << 8);
}

#define htonl(host) __khtonl((host))
#define htons(host) __khtons((host))

#define ntohl(net) __kntohl((net))
#define ntohs(net) __kntohs((net))

#define __kn32_make(n) ((n32) { .v = (n) })
#define __kn16_make(n) ((n16) { .v = (n) })

#define __kn32_to_uint32(n) ((n).v)
#define __kn16_to_uint16(n) ((n).v)

#define n32_make(n) __kn32_make((n))
#define n16_make(n) __kn16_make((n))

#define n32_to_uint32(n) __kn32_to_uint32((n))
#define n16_to_uint16(n) __kn16_to_uint16((n))

#define n32_equal(n1, n2) ((n1).v == (n2).v)
#define n16_equal(n1, n2) ((n1).v == (n2).v)

#define n32_nonzero(n) ((n).v != 0)
#define n16_nonzero(n) ((n).v != 0)

#endif
