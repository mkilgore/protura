/*
 * Copyright (C) 2017 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/mm/kmalloc.h>
#include <protura/snprintf.h>

#include <protura/fs/procfs.h>
#include <protura/net/arphrd.h>
#include <protura/net/arp.h>
#include <protura/net.h>
#include <protura/drivers/lo.h>

static struct net_interface loopback_iface = NET_INTERFACE_INIT(loopback_iface);

static void loopback_packet_send(struct net_interface *iface, struct packet *packet)
{
    packet->iface_rx = netdev_dup(iface);
    net_packet_receive(packet);
}

void net_loopback_init(void)
{
    loopback_iface.hard_tx = loopback_packet_send;
    loopback_iface.linklayer_tx = arp_tx;
    loopback_iface.hwtype = ARPHRD_LOOPBACK;

    loopback_iface.name = "lo";
    strcpy(loopback_iface.netdev_name, "lo");
    flag_set(&loopback_iface.flags, NET_IFACE_LOOPBACK);

    net_interface_register(&loopback_iface);
}

