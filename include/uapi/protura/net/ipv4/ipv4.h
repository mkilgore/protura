/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef __INCLUDE_UAPI_PROTURA_NET_IPV4_IPV4_H__
#define __INCLUDE_UAPI_PROTURA_NET_IPV4_IPV4_H__

#include <protura/net/sockaddr.h>

typedef __kn32 in_addr_t;
typedef __kn16 in_port_t;

struct in_addr {
    in_addr_t s_addr;
};

struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t   sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

#define IPPROTO_IP   0
#define IPPROTO_ICMP 1
#define IPPROTO_TCP  6
#define IPPROTO_UDP  17
#define IPOROTO_RAW  255
#define IPOROTO_MAX  256

#define INADDR_ANY       ((in_addr_t) __kn32_make(0x00000000))
#define INADDR_BROADCAST ((in_addr_t) __kn32_make(0xFFFFFFFF))

#endif
