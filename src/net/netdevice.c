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
#include <protura/rwlock.h>
#include <protura/snprintf.h>
#include <protura/list.h>
#include <arch/asm.h>

#include <protura/fs/procfs.h>
#include <protura/drivers/pci.h>
#include <protura/drivers/pci_ids.h>
#include <protura/net.h>
#include <protura/net/arphrd.h>
#include <protura/net/netdevice.h>

mutex_t net_interface_list_lock = MUTEX_INIT(net_interface_list_lock, "net-interface-list-lock");
list_head_t net_interface_list = LIST_HEAD_INIT(net_interface_list);

static struct net_interface *__find_netdev_name(const char *name, int *index)
{
    int c = 0;
    struct net_interface *net;

    list_foreach_entry(&net_interface_list, net, iface_entry) {
        if (strcmp(net->netdev_name, name) == 0) {
            if (index)
                *index = c;
            return net;
        }
        c++;
    }

    if (index)
        *index = -1;

    return NULL;
}

struct net_interface *netdev_get(const char *name)
{
    struct net_interface *iface;

    using_mutex(&net_interface_list_lock) {
        iface = __find_netdev_name(name, NULL);
        if (iface)
            atomic_inc(&iface->refs);
    }

    return iface;
}

void netdev_put(struct net_interface *iface)
{
    atomic_dec(&iface->refs);
}

static int ifreq_get_hwaddr(struct ifreq *ifreq, struct net_interface *iface)
{
    struct sockaddr_ether *ether = (struct sockaddr_ether *)&ifreq->ifr_hwaddr;

    using_netdev_read(iface) {
        ether->sa_family = iface->hwtype;
        memcpy(ether->sa_mac, iface->mac, 6);
    }

    return 0;
}

static int ifreq_set_hwaddr(struct ifreq *ifreq, struct net_interface *iface)
{
    return -ENOTSUP;
}

static int ifreq_set_addr(struct ifreq *ifreq, struct net_interface *iface)
{
    struct sockaddr_in *inet = (struct sockaddr_in *)&ifreq->ifr_addr;

    using_netdev_write(iface) {
        iface->in_addr = inet->sin_addr.s_addr;
        kp(KP_NORMAL, "%s: New IPv4: "PRin_addr"\n", iface->netdev_name, Pin_addr(iface->in_addr));
    }

    return 0;
}

static int ifreq_get_addr(struct ifreq *ifreq, struct net_interface *iface)
{
    struct sockaddr_in *inet = (struct sockaddr_in *)&ifreq->ifr_addr;

    using_netdev_read(iface) {
        inet->sin_family = AF_INET;
        inet->sin_addr.s_addr = iface->in_addr;
    }

    return 0;
}

static int ifreq_set_netmask(struct ifreq *ifreq, struct net_interface *iface)
{
    struct sockaddr_in *inet = (struct sockaddr_in *)&ifreq->ifr_netmask;

    using_netdev_write(iface)
        iface->in_netmask = inet->sin_addr.s_addr;

    return 0;
}

static int ifreq_get_netmask(struct ifreq *ifreq, struct net_interface *iface)
{
    struct sockaddr_in *inet = (struct sockaddr_in *)&ifreq->ifr_netmask;

    using_netdev_read(iface) {
        inet->sin_family = AF_INET;
        inet->sin_addr.s_addr = iface->in_netmask;
    }

    return 0;
}

static int ifreq_set_flags(struct ifreq *ifreq, struct net_interface *iface)
{
    using_netdev_write(iface) {
        if (ifreq->ifr_flags & IFF_UP)
            flag_set(&iface->flags, NET_IFACE_UP);
        else
            flag_clear(&iface->flags, NET_IFACE_UP);
    }

    return 0;
}

static int ifreq_get_flags(struct ifreq *ifreq, struct net_interface *iface)
{
    ifreq->ifr_flags = 0;
    using_netdev_read(iface) {
        ifreq->ifr_flags |= flag_test(&iface->flags, NET_IFACE_UP)? IFF_UP: 0;
        ifreq->ifr_flags |= flag_test(&iface->flags, NET_IFACE_LOOPBACK)? IFF_LOOPBACK: 0;
    }

    return 0;
}

static int ifreq_get_metrics(struct ifreq *ifreq, struct net_interface *iface)
{
    using_netdev_read(iface)
        ifreq->ifr_metrics = iface->metrics;

    return 0;
}

static int ifrequest2(int cmd, struct ifreq *ifreq)
{
    int ret = -ENOTSUP;
    struct net_interface *iface;

    iface = netdev_get(ifreq->ifr_name);

    if (!iface)
        return -ENODEV;

    switch (cmd) {
    case SIOCGIFHWADDR:
        ret = ifreq_get_hwaddr(ifreq, iface);
        break;

    case SIOCSIFHWADDR:
        ret = ifreq_set_hwaddr(ifreq, iface);
        break;

    case SIOCGIFADDR:
        ret = ifreq_get_addr(ifreq, iface);
        break;

    case SIOCSIFADDR:
        ret = ifreq_set_addr(ifreq, iface);
        break;

    case SIOCGIFNETMASK:
        ret = ifreq_get_netmask(ifreq, iface);
        break;

    case SIOCSIFNETMASK:
        ret = ifreq_set_netmask(ifreq, iface);
        break;

    case SIOCGIFFLAGS:
        ret = ifreq_get_flags(ifreq, iface);
        break;

    case SIOCSIFFLAGS:
        ret = ifreq_set_flags(ifreq, iface);
        break;

    case SIOCGIFMETRICS:
        ret= ifreq_get_metrics(ifreq, iface);
        break;
    }

    netdev_put(iface);

    return ret;
}

static int netdev_ioctl(struct file *filp, int cmd, uintptr_t ptr)
{
    int ret;
    int i;
    struct ifreq *addr;
    struct net_interface *iface;
    list_node_t *node;

    switch (cmd) {
    case SIOCGIFNAME:
        addr = (struct ifreq *)ptr;
        ret = user_check_region(addr, sizeof(*addr), F(VM_MAP_READ) | F(VM_MAP_WRITE));
        if (ret)
            return ret;

        using_mutex(&net_interface_list_lock) {
            node = &net_interface_list;
            for (i = 0; i <= addr->ifr_index; i++) {
                node = __list_first(node);
                if (list_ptr_is_head(&net_interface_list, node))
                    return -ENODEV;
            }

            iface = container_of(node, struct net_interface, iface_entry);
            strcpy(addr->ifr_name, iface->netdev_name);
            kp(KP_NORMAL, "Found iface: %s, %s, %d\n", iface->netdev_name, addr->ifr_name, addr->ifr_index);
        }
        break;

    case SIOCGIFINDEX:
        addr = (struct ifreq *)ptr;
        ret = user_check_region(addr, sizeof(*addr), F(VM_MAP_READ) | F(VM_MAP_WRITE));
        if (ret)
            return ret;

        using_mutex(&net_interface_list_lock)
            __find_netdev_name(addr->ifr_name, &addr->ifr_index);

        break;

    default:
        addr = (struct ifreq *)ptr;
        ret = user_check_region(addr, sizeof(*addr), F(VM_MAP_READ) | F(VM_MAP_WRITE));
        if (ret)
            return ret;

        return ifrequest2(cmd, addr);
    }
    return 0;
}

static int netdevice_read(void *page, size_t page_size, size_t *len)
{
    struct net_interface *net;
    *len = 0;

    using_mutex(&net_interface_list_lock) {
        list_foreach_entry(&net_interface_list, net, iface_entry) {
            *len += snprintf(page + *len, page_size - *len,
                    "%s: %d\n", net->netdev_name, atomic_get(&net->refs));
        }
    }

    return 0;
}

struct procfs_entry_ops netdevice_procfs = {
    .ioctl = netdev_ioctl,
    .readpage = netdevice_read,
};

struct net_interface *netdev_get_inet(in_addr_t inet_addr)
{
    struct net_interface *net;
    struct net_interface *iface = NULL;
    using_mutex(&net_interface_list_lock) {
        list_foreach_entry(&net_interface_list, net, iface_entry) {
            if (n32_equal(inet_addr, net->in_addr)) {
                iface = netdev_dup(net);
                break;
            }
        }
    }

    return iface;
}

struct net_interface *netdev_get_network(in_addr_t inet_addr)
{
    struct net_interface *net;
    struct net_interface *iface = NULL;
    using_mutex(&net_interface_list_lock) {
        list_foreach_entry(&net_interface_list, net, iface_entry) {
            if (n32_equal(in_addr_mask(inet_addr, net->in_netmask), in_addr_mask(net->in_addr, net->in_netmask))) {
                iface = netdev_dup(net);
                break;
            }
        }
    }

    return iface;
}

struct net_interface *netdev_get_hwaddr(uint8_t *mac, size_t len)
{
    struct net_interface *net;
    struct net_interface *iface = NULL;

    using_mutex(&net_interface_list_lock) {
        list_foreach_entry(&net_interface_list, net, iface_entry) {
            if (memcmp(net->mac, mac, len) == 0) {
                iface = netdev_dup(net);
                break;
            }
        }
    }

    return iface;
}

static int eth_next = 0;

void net_interface_register(struct net_interface *iface)
{
    using_mutex(&net_interface_list_lock) {
        if (!iface->netdev_name[0])
            snprintf(iface->netdev_name, sizeof(iface->netdev_name), "%s%d", iface->name, eth_next++);
        list_add_tail(&net_interface_list, &iface->iface_entry);
    }

    kp(KP_NORMAL, "Registered netdev interface: %s\n", iface->netdev_name);
}

