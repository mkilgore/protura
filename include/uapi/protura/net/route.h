/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef __INCLUDE_UAPI_PROTURA_NET_ROUTE_H__
#define __INCLUDE_UAPI_PROTURA_NET_ROUTE_H__

#include <protura/types.h>
#include <protura/net/sockaddr.h>
#include <protura/net/if.h>

struct route_entry {
    __kuint32_t flags;

    struct sockaddr dest;
    struct sockaddr gateway;
    struct sockaddr netmask;

    char netdev[IFNAMSIZ];
};

#define RT_ENT_GATEWAY 0x01
#define RT_ENT_UP      0x02

#define SIOCADDRT 0x8980
#define SIOCDELRT 0x8981

#endif
