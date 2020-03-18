/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef __INCLUDE_UAPI_PROTURA_NET_SOCKADDR_H__
#define __INCLUDE_UAPI_PROTURA_NET_SOCKADDR_H__

#include <protura/types.h>
#include <protura/net/types.h>

struct sockaddr {
    unsigned short    sa_family;    // address family, AF_xxx
    char              sa_data[14];  // 14 bytes of protocol address
};

typedef __ksize_t   socklen_t;
typedef __kuint16_t sa_family_t;

#define AF_INET 2
#define AF_MAX 3

#define PF_INET AF_INET
#define PF_MAX  AF_MAX

#define NET_MAXID AF_MAX

#define SOL_SOCKET 1

#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#define SOCK_RAW    3

#define SOCK_NONBLOCK 0x4000
#define SOCK_CLOEXEC  0x40000

#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2

#endif
