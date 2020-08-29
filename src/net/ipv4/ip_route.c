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
#include <protura/dump_mem.h>
#include <protura/mm/kmalloc.h>
#include <protura/snprintf.h>
#include <protura/list.h>
#include <arch/asm.h>

#include <protura/net/route.h>
#include <protura/net.h>
#include <protura/net/arp.h>
#include <protura/net/ipv4/ipv4.h>
#include <protura/net/ipv4/ip_route.h>
#include "ipv4.h"

struct ip_forward_route {
    list_node_t route_entry;

    n32 dest_ip;
    n32 gateway_ip;
    struct net_interface *iface;

    flags_t flags;
};

struct ip_forward_zone {
    list_head_t route_list;
    n32 mask;
};

struct ip_forward_table {
    struct ip_forward_zone zones[33];
};

static mutex_t forward_table_lock = MUTEX_INIT(forward_table_lock);
static struct ip_forward_table forward_table;

/*
 * Simple functions for working with netmasks
 *
 * 'count' refers to the number of one's in the netmask
 *
 * So 255.255.255.0 is count 24.
 */
static int netmask_count(n32 mask)
{
    if (!n32_to_uint32(mask))
        return 0;

    uint32_t umask = ntohl(mask);
    return 32 - bit32_find_first_set(umask);
}

static n32 netmask_create(int count)
{
    uint32_t mask;

    if (count == 0)
        return htonl(0);

    mask = ~((1 << (32 - count)) - 1);
    return htonl(mask);
}

void ip_route_init(void)
{
    int i;
    for (i = 0; i < 33; i++) {
        list_head_init(&forward_table.zones[i].route_list);
        forward_table.zones[i].mask = netmask_create(i);
    }
}

void ip_route_add(n32 dest_ip, n32 gateway_ip, n32 netmask, struct net_interface *iface, flags_t flags)
{
    int count = netmask_count(netmask);
    struct ip_forward_route *route = kzalloc(sizeof(*route), PAL_KERNEL);

    list_node_init(&route->route_entry);
    route->dest_ip = dest_ip;
    route->gateway_ip = gateway_ip;
    route->iface = netdev_dup(iface);
    route->flags = flags;

    kp_ip_trace("Adding route for netmask: "PRin_addr"\n", Pin_addr(netmask));

    using_mutex(&forward_table_lock)
        list_add_tail(&forward_table.zones[count].route_list, &route->route_entry);
}

int ip_route_del(n32 dest_ip, n32 netmask)
{
    int count = netmask_count(netmask);
    struct ip_forward_route *route = NULL;

    using_mutex(&forward_table_lock) {
        struct ip_forward_route *entry;
        kp_ip_trace("Netmask count: %d\n", count);

        list_foreach_entry(&forward_table.zones[count].route_list, entry, route_entry) {
            kp_ip_trace("Entry entry & netmask: "PRin_addr", dest & netmask: "PRin_addr"\n",
                    Pin_addr(in_addr_mask(entry->dest_ip, netmask)), Pin_addr(in_addr_mask(dest_ip, netmask)));

            if (n32_equal(in_addr_mask(entry->dest_ip, netmask), in_addr_mask(dest_ip, netmask))) {
                kp_ip_trace("Found route\n");
                route = entry;
                list_del(&route->route_entry);
                break;
            }
        }
    }

    if (!route)
        return -ENODEV;

    netdev_put(route->iface);
    kfree(route);

    return 0;
}

int ip_route_get(n32 dest_ip, struct ip_route_entry *ret)
{
    int i;
    struct ip_forward_route *route, *found = NULL;

    using_mutex(&forward_table_lock) {
        for (i = 32; i >= 0 && !found; i--) {
            n32 mask = forward_table.zones[i].mask;
            list_foreach_entry(&forward_table.zones[i].route_list, route, route_entry) {
                if (!flag_test(&route->iface->flags, NET_IFACE_UP))
                    continue;

                if (n32_equal(in_addr_mask(route->dest_ip, mask), in_addr_mask(dest_ip, mask))) {
                    found = route;
                    break;
                }
            }
        }

        if (found) {
            ip_route_entry_init(ret);

            ret->flags = found->flags;
            ret->dest_ip = dest_ip;
            ret->gateway_ip = found->gateway_ip;
            ret->iface = netdev_dup(found->iface);
        }
    }

    if (found)
        return 0;
    else
        return -EACCES;
}

void ip_route_clear(struct ip_route_entry *entry)
{
    if (entry->iface)
        netdev_put(entry->iface);

    entry->iface = NULL;
}

static int ip_route_readpage(void *page, size_t page_size, size_t *len)
{
    struct route_entry new_ent;
    struct ip_forward_route *route;
    int i;

    *len = 0;

    using_mutex(&forward_table_lock) {
        for (i = 0; i <= 32; i++) {
            n32 mask = forward_table.zones[i].mask;

            list_foreach_entry(&forward_table.zones[i].route_list, route, route_entry) {
                memset(&new_ent, 0, sizeof(new_ent));

                sockaddr_in_assign(&new_ent.dest, route->dest_ip, htons(0));

                if (flag_test(&route->flags, IP_ROUTE_GATEWAY)) {
                    new_ent.flags |= RT_ENT_GATEWAY;

                    sockaddr_in_assign(&new_ent.gateway, route->gateway_ip, htons(0));
                }

                if (flag_test(&route->iface->flags, NET_IFACE_UP))
                    new_ent.flags |= RT_ENT_UP;

                sockaddr_in_assign(&new_ent.netmask, mask, htons(0));

                using_netdev_read(route->iface)
                    memcpy(&new_ent.netdev, route->iface->netdev_name, sizeof(new_ent.netdev));

                memcpy(page + *len, &new_ent, sizeof(new_ent));
                *len += sizeof(new_ent);

                if (page_size - *len < sizeof(new_ent))
                    goto done;
            }
        }
    }

  done:
    return 0;
}

static int ip_route_ioctl(struct file *filp, int cmd, struct user_buffer ptr)
{
    struct route_entry tmp_ent;
    struct net_interface *iface;
    struct sockaddr_in *dest, *gateway, *netmask;
    flags_t flags = 0;
    int ret = -ENOTSUP;

    switch (cmd) {
    case SIOCADDRT:
        ret = user_copy_to_kernel(&tmp_ent, ptr);
        if (ret)
            return ret;

        dest = (struct sockaddr_in *)&tmp_ent.dest;
        gateway = (struct sockaddr_in *)&tmp_ent.gateway;
        netmask = (struct sockaddr_in *)&tmp_ent.netmask;
        if (tmp_ent.flags & RT_ENT_GATEWAY)
            flag_set(&flags, IP_ROUTE_GATEWAY);

        iface = netdev_get(tmp_ent.netdev);
        if (!iface)
            return -ENODEV;

        ip_route_add(dest->sin_addr.s_addr, gateway->sin_addr.s_addr, netmask->sin_addr.s_addr, iface, flags);

        netdev_put(iface);
        return 0;

    case SIOCDELRT:
        ret = user_copy_to_kernel(&tmp_ent, ptr);
        if (ret)
            return ret;

        dest = (struct sockaddr_in *)&tmp_ent.dest;
        netmask = (struct sockaddr_in *)&tmp_ent.netmask;

        return ip_route_del(dest->sin_addr.s_addr, netmask->sin_addr.s_addr);
    }

    return ret;
}

struct procfs_entry_ops ipv4_route_ops = {
    .readpage = ip_route_readpage,
    .ioctl = ip_route_ioctl,
};

