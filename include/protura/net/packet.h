#ifndef INCLUDE_PROTURA_NET_PACKET_H
#define INCLUDE_PROTURA_NET_PACKET_H

#include <protura/types.h>
#include <protura/list.h>
#include <protura/bits.h>
#include <protura/rwlock.h>
#include <protura/work.h>
#include <protura/string.h>
#include <protura/net/sockaddr.h>
#include <protura/net/ipv4/ipv4.h>

struct net_interface;

/*
enum packet_flags {
};
 */

struct packet {
    list_node_t packet_entry;
    flags_t flags;

    struct delay_work dwork;

    /* Data-link layer */
    struct net_interface *iface_tx;
    struct net_interface *iface_rx;

    /* address-resolution layer */
    n16 ll_type;
    char dest_mac[6];

    /* Address-family layer */
    in_addr_t route_addr;
    n16 protocol_type;

    /* Protocol layer */
    struct sockaddr src_addr;
    socklen_t src_len;

    struct sockaddr dest_addr;
    socklen_t dest_len;

    struct page *page;
    void *start, *head, *tail, *end;

    void *ll_head, *af_head, *proto_head;

    struct socket *sock;
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

void net_packet_receive(struct packet *);
void net_packet_transmit(struct packet *);

struct packet *packet_new(void);
void packet_free(struct packet *);
struct packet *packet_copy(struct packet *);

void packet_add_header(struct packet *, const void *header, size_t header_len);
void packet_append_data(struct packet *, const void *data, size_t data_len);
void packet_pad_zero(struct packet *packet, size_t len);

static inline size_t packet_len(struct packet *packet)
{
    return packet->tail - packet->head;
}

static inline void packet_to_buffer(struct packet *packet, void *buf, size_t buf_len)
{
    size_t plen = packet_len(packet);
    size_t len = (buf_len < plen)? plen: buf_len;
    memcpy(buf, packet->head, len);
}

#endif
