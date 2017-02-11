#ifndef INCLUDE_PROTURA_NET_H
#define INCLUDE_PROTURA_NET_H

#include <protura/types.h>
#include <protura/list.h>
#include <protura/bits.h>
#include <protura/rwlock.h>
#include <protura/string.h>
#include <protura/socket.h>
#include <protura/net/netdevice.h>
#include <protura/net/inet.h>

struct net_interface;

/*
enum packet_flags {
};
*/

struct packet {
    list_node_t packet_entry;
    flags_t flags;

    struct net_interface *iface_tx;

    uint8_t mac_dest[6];
    uint16_t ll_type;

    struct page *page;
    void *start, *head, *tail, *end;
};

#define PACKET_RESERVE_HEADER_SPACE 1024

#define PACKET_INIT(packet) \
    { \
        .packet_entry = LIST_NODE_INIT((packet).packet_entry), \
    }

static inline void packet_init(struct packet *packet)
{
    *packet = (struct packet)PACKET_INIT(*packet);
}

struct net_interface {
    list_node_t iface_entry;

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

void net_packet_receive(struct packet *);

struct packet *packet_new(void);
void packet_free(struct packet *);
struct packet *packet_dup(struct packet *);

void packet_add_header(struct packet *, void *header, size_t header_len);
void packet_append_data(struct packet *, void *data, size_t data_len);
void packet_pad_zero(struct packet *packet, size_t len);

static inline size_t packet_len(struct packet *packet)
{
    return packet->tail - packet->head;
}

static inline void packet_to_buffer(struct packet *packet, void *buf, size_t buf_len)
{
    size_t len = (buf_len < packet_len(packet))? packet_len(packet): buf_len;
    memcpy(buf, packet->head, len);
}

void packet_linklayer_rx(struct packet *packet);
void packet_linklayer_tx(struct packet *packet);

void arp_handle_packet(struct packet *packet);
int arp_ipv4_to_mac(in_addr_t inet_addr, uint8_t *mac, struct net_interface *iface);

#endif
