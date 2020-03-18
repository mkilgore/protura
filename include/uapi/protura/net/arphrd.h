/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef __INCLUDE_UAPI_PROTURA_NET_ARPHRD_H__
#define __INCLUDE_UAPI_PROTURA_NET_ARPHRD_H__

#define ETH_P_ARP 0x0806
#define ETH_P_IP  0x0800

#define ARPHRD_ETHER 1
#define ARPHRD_LOOPBACK 772

#define	ARPOP_REQUEST 1
#define	ARPOP_REPLY   2

struct sockaddr_ether {
    unsigned short sa_family;
    char sa_mac[6]; /* Mac address */
    char sa_data[8];  /* Padding */
};

#endif
