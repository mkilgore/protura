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

#include <protura/net/socket.h>
#include <protura/net.h>
#include "ipv4.h"

struct udp_header {
    uint16_t source_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __packed;

#define UDP_HASH_SIZE 128

#define UDP_LOWEST_AUTOBIND_PORT 50000

struct udp_protocol {
    struct protocol proto;

    mutex_t lock;
    hlist_head_t listening_ports[UDP_HASH_SIZE];
    uint16_t next_port;
};

static struct protocol_ops udp_protocol_ops;

static struct udp_protocol udp_protocol = {
    .proto = PROTOCOL_INIT(udp_protocol.proto, PROTOCOL_UDP, &udp_protocol_ops),
    .lock = MUTEX_INIT(udp_protocol.lock, "udp-protocol-lock"),
    .next_port = UDP_LOWEST_AUTOBIND_PORT,
};


static int udp_hash_port(n16 port)
{
    return port % UDP_HASH_SIZE;
}

static n16 udp_find_port(struct udp_protocol *proto)
{
    n16 port;
    int found = 0;

    using_mutex(&proto->lock) {
        do {
            struct socket *sock;
            int hash;

            port = htons(proto->next_port++);

            if (proto->next_port == 65536)
                proto->next_port = UDP_LOWEST_AUTOBIND_PORT;

            hash = udp_hash_port(port);

            found = 1;
            hlist_foreach_entry(&proto->listening_ports[hash], sock, socket_hash_entry) {
                struct udp_socket_private *priv = &sock->proto_private.udp;
                if (priv->src_port == port) {
                    found = 0;
                    break;
                }
            }
        } while (!found);
    }

    kp_udp("Autobind port: %d\n", ntohs(port));
    return port;
}

static int udp_register_sock(struct udp_protocol *proto, struct socket *sock)
{
    n16 port = sock->proto_private.udp.src_port;
    int hash = udp_hash_port(port);
    struct socket *s;
    int ret = 0;

    using_mutex(&proto->lock) {
        hlist_foreach_entry(&proto->listening_ports[hash], s, socket_hash_entry) {
            if (s->proto_private.udp.src_port == port) {
                ret = -EADDRINUSE;
                break;
            }
        }

        if (!ret)
            hlist_add(&proto->listening_ports[hash], &socket_dup(sock)->socket_hash_entry);
    }

    return ret;
}

static void udp_unregister_port(struct udp_protocol *proto, n16 port)
{
    int hash = udp_hash_port(port);
    struct socket *s;

    using_mutex(&proto->lock) {
        hlist_foreach_entry(&proto->listening_ports[hash], s, socket_hash_entry) {
            if (s->proto_private.udp.src_port == port) {
                hlist_del(&s->socket_hash_entry);
                break;
            }
        }
    }

    if (s)
        socket_put(s);
}

void udp_rx(struct protocol *proto, struct packet *packet)
{
    struct udp_protocol *udp = container_of(proto, struct udp_protocol, proto);
    struct udp_header *header = packet->head;
    struct sockaddr_in *in;
    struct socket *sock, *found = NULL;
    int hash;

    in = (struct sockaddr_in *)&packet->src_addr;
    in->sin_port = header->source_port;

    kp_udp(" %d -> %d, %d bytes\n", ntohs(header->source_port), ntohs(header->dest_port), ntohs(header->length));

    /* Manually set the length according to the UDP length field */
    packet->tail = packet->head + ntohs(header->length);
    packet->head += sizeof(*header);

    hash = udp_hash_port(header->dest_port);

    using_mutex(&udp->lock) {
        hlist_foreach_entry(&udp->listening_ports[hash], sock, socket_hash_entry) {
            struct udp_socket_private *priv = &sock->proto_private.udp;
            if (priv->src_port == header->dest_port) {
                found = socket_dup(sock);
                break;
            }
        }
    }

    if (found) {
        kp_udp("Found bound socket\n");
        using_mutex(&sock->recv_lock) {
            list_add_tail(&sock->recv_queue, &packet->packet_entry);
            wait_queue_wake(&sock->recv_wait_queue);
        }

        socket_put(found);
    } else {
        packet_free(packet);
    }
}

int udp_tx(struct packet *packet)
{
    const struct sockaddr_in *in;
    size_t plen;
    struct udp_header *header;
    struct socket *sock = packet->sock;
    struct address_family *af = sock->af;

    packet->head -= sizeof(*header);
    header = packet->head;

    plen = packet_len(packet);

    in = (struct sockaddr_in *)&packet->src_addr;
    header->source_port = in->sin_port;

    in = (struct sockaddr_in *)&packet->dest_addr;
    header->dest_port = in->sin_port;

    header->length = htons(plen);
    header->checksum = 0;

    packet->protocol_type = IPPROTO_UDP;

    return af->ops->packet_tx(af, packet);
}

static int udp_sendto(struct protocol *protocol, struct socket *sock, struct packet *packet, const struct sockaddr *addr, socklen_t len)
{
    const struct sockaddr_in *in;

    if (sizeof(*in) > len)
        return -EFAULT;

    in = (const struct sockaddr_in *)addr;

    sockaddr_in_assign(&packet->dest_addr, in->sin_addr.s_addr, in->sin_port);
    sockaddr_in_assign(&packet->src_addr, 0, sock->proto_private.udp.src_port);

    packet->sock = socket_dup(sock);

    sock->af->ops->process_sockaddr(sock->af, packet, addr, len);
    return udp_tx(packet);
}

static int udp_bind(struct protocol *protocol, struct socket *sock, const struct sockaddr *addr, socklen_t len)
{
    struct udp_protocol *udp = container_of(protocol, struct udp_protocol, proto);
    const struct sockaddr_in *in = (const struct sockaddr_in *)addr;
    int ret;

    if (len < sizeof(*in))
        return -EFAULT;

    sock->proto_private.udp.src_port = in->sin_port;

    ret = udp_register_sock(udp, sock);
    if (ret) {
        sock->proto_private.udp.src_port = 0;
        return ret;
    }

    return 0;
}

static int udp_autobind(struct protocol *protocol, struct socket *sock)
{
    int ret;
    struct udp_protocol *udp = container_of(protocol, struct udp_protocol, proto);

    sock->proto_private.udp.src_port = udp_find_port(udp);

    ret = udp_register_sock(udp, sock);
    if (ret) {
        sock->proto_private.udp.src_port = 0;
        return ret;
    }

    return 0;
}

static int udp_create(struct protocol *protocol, struct socket *sock)
{
    return 0;
}

static int udp_delete(struct protocol *protocol, struct socket *sock)
{
    struct udp_protocol *udp = container_of(protocol, struct udp_protocol, proto);

    if (sock->proto_private.udp.src_port)
        udp_unregister_port(udp, sock->proto_private.udp.src_port);

    return 0;
}

static int udp_getsockname(struct protocol *protocol, struct socket *sock, struct sockaddr *addr, socklen_t *len)
{
    struct sockaddr_in *in = (struct sockaddr_in *)addr;

    if (*len < sizeof(*in))
        return -EFAULT;

    in->sin_port = sock->proto_private.udp.src_port;
    *len = sizeof(*in);

    return 0;
}

static struct protocol_ops udp_protocol_ops = {
    .packet_rx = udp_rx,
    .sendto = udp_sendto,
    .bind = udp_bind,
    .autobind = udp_autobind,
    .getsockname = udp_getsockname,

    .create = udp_create,
    .delete = udp_delete,
};

void udp_init(void)
{
    protocol_register(&udp_protocol.proto);
}

