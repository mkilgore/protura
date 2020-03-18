/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef __INCLUDE_UAPI_PROTURA_NET_IF_H__
#define __INCLUDE_UAPI_PROTURA_NET_IF_H__

#include <protura/types.h>
#include <protura/net/sockaddr.h>

#define IFNAMSIZ 20

#define SIOCGIFNAME 0x8901
#define SIOCGIFINDEX 0x8902

#define SIOCGIFHWADDR 0x8903
#define SIOCSIFHWADDR 0x8904

#define SIOCGIFADDR   0x8905
#define SIOCSIFADDR   0x8906

#define SIOCGIFNETMASK 0x8907
#define SIOCSIFNETMASK 0x8908

#define SIOCGIFFLAGS 0x8909
#define SIOCSIFFLAGS 0x890A

#define SIOCGIFMETRICS 0x890B

struct ifmetrics {
    __kuint32_t rx_packets;
    __kuint32_t tx_packets;
    __kuint64_t rx_bytes;
    __kuint64_t tx_bytes;
};

struct ifreq {
    char ifr_name[IFNAMSIZ];
    union {
        struct sockaddr ifr_addr;
        struct sockaddr ifr_hwaddr;
        struct sockaddr ifr_netmask;
        int ifr_flags;
        int ifr_index;

        struct ifmetrics ifr_metrics;
    };
};

#define IFF_UP 0x0001
#define IFF_LOOPBACK 0x0002

#endif
