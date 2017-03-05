#ifndef INCLUDE_PROTURA_NET_ROUTE_H
#define INCLUDE_PROTURA_NET_ROUTE_H

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
