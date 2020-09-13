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
#include <protura/kparam.h>

#include <protura/fs/procfs.h>
#include <protura/net/socket.h>
#include <protura/net/ipv4/ipv4.h>
#include <protura/net.h>
#include "ipv4/ipv4.h"

int ip_max_log_level = CONFIG_IP_LOG_LEVEL;
int icmp_max_log_level = CONFIG_ICMP_LOG_LEVEL;
int udp_max_log_level = CONFIG_UDP_LOG_LEVEL;
int tcp_max_log_level = CONFIG_TCP_LOG_LEVEL;

KPARAM("ip.loglevel", &ip_max_log_level, KPARAM_LOGLEVEL);
KPARAM("icmp.loglevel", &icmp_max_log_level, KPARAM_LOGLEVEL);
KPARAM("udp.loglevel", &udp_max_log_level, KPARAM_LOGLEVEL);
KPARAM("tcp.loglevel", &tcp_max_log_level, KPARAM_LOGLEVEL);

struct procfs_dir *net_dir_procfs;

static void net_procfs_init(void)
{
    net_dir_procfs = procfs_register_dir(&procfs_root, "net");

    procfs_register_entry_ops(net_dir_procfs, "netdev", &netdevice_procfs);
    procfs_register_entry(net_dir_procfs, "sockets", &socket_procfs_file_ops);
}
initcall_device(net_procfs, net_procfs_init);
