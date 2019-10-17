#ifndef INCLUDE_PROTURA_NET_IP_ROUTE_H
#define INCLUDE_PROTURA_NET_IP_ROUTE_H

#include <protura/types.h>
#include <protura/atomic.h>
#include <protura/bits.h>
#include <protura/net/types.h>

struct net_interface;

enum ip_route_flags {
    IP_ROUTE_GATEWAY,
};

/*
 * Contains all the information necessary to route an IP packet to its destination.
 *
 * After this is created, should be treated as read-only as it may be accessed
 * through multiple different threads.
 */
struct ip_route_entry {
    flags_t flags;

    n32 dest_ip;
    n32 gateway_ip;

    struct net_interface *iface;
};

#define IP_ROUTE_ENTRY_INIT(route) \
    { \
    }

static inline void ip_route_entry_init(struct ip_route_entry *ent)
{
    *ent = (struct ip_route_entry)IP_ROUTE_ENTRY_INIT(*ent);
}

void ip_route_init(void);
void ip_route_add(n32 dest_ip, n32 gateway_ip, n32 netmask, struct net_interface *iface, flags_t flags);
int ip_route_del(n32 dest_ip, n32 netmask);

int ip_route_get(n32 dest_ip, struct ip_route_entry *ent);

static inline n32 ip_route_get_ip(struct ip_route_entry *ent)
{
    if (flag_test(&ent->flags, IP_ROUTE_GATEWAY))
        return ent->gateway_ip;
    else
        return ent->dest_ip;
}

void ip_route_clear(struct ip_route_entry *entry);

#endif
