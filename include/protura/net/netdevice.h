#ifndef INCLUDE_PROTURA_NET_NETDEVICE_H
#define INCLUDE_PROTURA_NET_NETDEVICE_H

#include <protura/types.h>
#include <protura/list.h>
#include <protura/bits.h>
#include <protura/rwlock.h>
#include <protura/string.h>
#include <protura/net/af/ipv4.h>
#include <protura/net/if.h>

struct packet;

enum net_interface_flags {
    NET_IFACE_UP,
};

struct net_interface {
    list_node_t iface_entry;
    flags_t flags;

    atomic_t refs;
    mutex_t lock;

    const char *name;
    char netdev_name[IFNAMSIZ];

    in_addr_t in_addr;
    in_addr_t in_netmask;
    in_addr_t in_broadcast;

    uint8_t mac[6];
    int (*packet_send) (struct net_interface *, struct packet *);
};

#define PRmac "%02x:%02x:%02x:%02x:%02x:%02x"
#define Pmac(m) m[0], m[1], m[2], m[3], m[4], m[5]

/* You must hold the lock when traversing net_interface_list.
 *
 * Note: DO NOT take this lock if you have a lock on a netdev. */
extern mutex_t net_interface_list_lock;
extern list_head_t net_interface_list;

/*
 * Gets the interface for which inet_addr is on the same network as it
 */
struct net_interface *netdev_get_inet(in_addr_t inet_addr);
struct net_interface *netdev_get_network(in_addr_t addr);
struct net_interface *netdev_get_hwaddr(uint8_t *mac, size_t len);

struct net_interface *netdev_get(const char *name);
void netdev_put(struct net_interface *);

static inline struct net_interface *netdev_dup(struct net_interface *iface)
{
    atomic_inc(&iface->refs);
    return iface;
}

static inline void netdev_lock_read(struct net_interface *iface)
{
    mutex_lock(&iface->lock);
}

static inline void netdev_unlock_read(struct net_interface *iface)
{
    mutex_unlock(&iface->lock);
}

#define using_netdev_read(net) \
    using_nocheck(netdev_lock_read(net), netdev_unlock_read(net))

static inline void netdev_lock_write(struct net_interface *iface)
{
    mutex_lock(&iface->lock);
}

static inline void netdev_unlock_write(struct net_interface *iface)
{
    mutex_unlock(&iface->lock);
}

#define using_netdev_write(net) \
    using_nocheck(netdev_lock_write(net), netdev_unlock_write(net))

void net_interface_register(struct net_interface *iface);
void net_init(void);

#endif
