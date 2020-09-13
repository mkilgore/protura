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

static void loopback_process_tx_queue(struct net_interface *iface)
{
    using_spinlock(&iface->tx_lock) {
        while (__net_iface_has_tx_packet(iface)) {
            struct packet *packet = __net_iface_tx_packet_pop(iface);

            packet->iface_rx = netdev_dup(iface);
            net_packet_receive(packet);
        }
    }
}

static void net_loopback_init(void)
{
    loopback_iface.process_tx_queue = loopback_process_tx_queue;
    loopback_iface.linklayer_tx = arp_tx;
    loopback_iface.hwtype = ARPHRD_LOOPBACK;

    loopback_iface.name = "lo";
    strcpy(loopback_iface.netdev_name, "lo");
    flag_set(&loopback_iface.flags, NET_IFACE_LOOPBACK);

    net_interface_register(&loopback_iface);
}
initcall_device(net_loopback, net_loopback_init);
