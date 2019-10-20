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
#include <protura/list.h>
#include <arch/asm.h>

#include <protura/fs/procfs.h>
#include <protura/drivers/lo.h>
#include <protura/net/ipv4/ip_route.h>
#include <protura/net/socket.h>
#include <protura/net/linklayer.h>
#include <protura/net/arp.h>
#include <protura/net/ipv4/ipv4.h>
#include <protura/net/ipv4/udp.h>
#include <protura/net/ipv4/icmp.h>
#include <protura/net.h>
#include "ipv4/ipv4.h"
#include "internal.h"

struct procfs_dir *net_dir_procfs;

void net_init(void)
{
    net_dir_procfs = procfs_register_dir(&procfs_root, "net");

    net_loopback_init();
    net_packet_queue_init();
    ip_route_init();
    socket_subsystem_init();

    arp_init();

    ip_init();

    address_family_setup();
    linklayer_setup();

    icmp_init();

    procfs_register_entry_ops(net_dir_procfs, "netdev", &netdevice_procfs);
    procfs_register_entry(net_dir_procfs, "sockets", &socket_procfs_file_ops);
}

