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
    n16 source_port;
    n16 dest_port;
    n16 length;
    n16 checksum;
} __packed;

#define UDP_HASH_SIZE 128

#define UDP_LOWEST_AUTOBIND_PORT 50000

struct udp_protocol {
    struct protocol proto;

    mutex_t lock;
    uint16_t next_port;
};

static struct protocol_ops udp_protocol_ops;

static struct udp_protocol udp_protocol = {
    .proto = PROTOCOL_INIT(&udp_protocol_ops),
    .lock = MUTEX_INIT(udp_protocol.lock, "udp-protocol-lock"),
    .next_port = UDP_LOWEST_AUTOBIND_PORT,
};

static n16 udp_find_port(struct udp_protocol *proto)
{
    n16 port;
    int found = 0;

    using_mutex(&proto->lock) {
        do {
            port = htons(proto->next_port++);

            if (proto->next_port == 65536)
                proto->next_port = UDP_LOWEST_AUTOBIND_PORT;

            /* FIXME: check this against the address_family_ip socket table */
            found = 1;
        } while (!found);
    }

    kp_udp("Autobind port: %d\n", ntohs(port));
    return port;
}

static int udp_register_sock(struct udp_protocol *proto, struct socket *sock)
{
    int ret = 0;
    struct address_family_ip *af = container_of(sock->af, struct address_family_ip, af);

    struct ip_lookup test_lookup = {
        .proto = IPPROTO_UDP,
        .src_port = sock->af_private.ipv4.src_port,
        .src_addr = sock->af_private.ipv4.src_addr,
        .dest_port = sock->af_private.ipv4.dest_port,
        .dest_addr = sock->af_private.ipv4.dest_addr,
    };

    using_mutex(&af->lock) {
        struct socket *s = __ipaf_find_socket(af, &test_lookup, 4);
        if (s) {
            ret = -EADDRINUSE;
            break;
        }

        kp_udp("Adding socket, src_port: %d, src_addr: "PRin_addr", dest_port: %d, dest_addr: "PRin_addr"\n", ntohs(test_lookup.src_port), Pin_addr(test_lookup.src_addr), ntohs(test_lookup.dest_port), Pin_addr(test_lookup.dest_addr));

        __ipaf_add_socket(af, sock);
    }

    return ret;
}

void udp_rx(struct protocol *proto, struct packet *packet)
{
    struct udp_header *header = packet->head;
    struct sockaddr_in *in;
    struct socket *sock = packet->sock;

    in = (struct sockaddr_in *)&packet->src_addr;
    in->sin_port = header->source_port;

    kp_udp(" %d -> %d, %d bytes\n", ntohs(header->source_port), ntohs(header->dest_port), ntohs(header->length));

    /* Manually set the length according to the UDP length field */
    packet->tail = packet->head + ntohs(header->length);
    packet->head += sizeof(*header);

    using_mutex(&sock->recv_lock) {
        list_add_tail(&sock->recv_queue, &packet->packet_entry);
        wait_queue_wake(&sock->recv_wait_queue);
    }
}

int udp_tx(struct packet *packet)
{
    const struct sockaddr_in *in;
    size_t plen;
    struct udp_header *header;

    packet->head -= sizeof(*header);
    header = packet->head;

    plen = packet_len(packet);

    in = (struct sockaddr_in *)&packet->src_addr;
    header->source_port = in->sin_port;

    in = (struct sockaddr_in *)&packet->dest_addr;
    header->dest_port = in->sin_port;

    header->length = htons(plen);
    header->checksum = htons(0);

    packet->protocol_type = IPPROTO_UDP;

    return ip_tx(packet);
}

static int udp_process_sockaddr(struct packet *packet, const struct sockaddr *addr, socklen_t len)
{
    if (addr) {
        const struct sockaddr_in *in;

        if (sizeof(*in) > len)
            return -EFAULT;

        if (addr->sa_family != AF_INET)
            return -EINVAL;

        in = (const struct sockaddr_in *)addr;

        sockaddr_in_assign_port(&packet->dest_addr, in->sin_port);
    } else if (flag_test(&packet->sock->flags, SOCKET_IS_CONNECTED)) {
        sockaddr_in_assign_port(&packet->dest_addr, packet->sock->af_private.ipv4.dest_port);
    } else {
        return -EDESTADDRREQ;
    }

    return 0;
}

static int udp_sendto(struct protocol *protocol, struct socket *sock, struct packet *packet, const struct sockaddr *addr, socklen_t len)
{
    kp_udp("Processing sockaddr: %p\n", addr);
    int ret = udp_process_sockaddr(packet, addr, len);
    if (ret)
        return ret;

    kp_udp("Assigning packet src_port\n");
    sockaddr_in_assign(&packet->src_addr, INADDR_ANY, sock->af_private.ipv4.src_port);

    kp_udp("Processing packet IP sockaddr: %p\n", addr);
    ret = ip_process_sockaddr(packet, addr, len);
    if (ret)
        return ret;

    kp_udp("Packet Tx\n");
    return udp_tx(packet);
}

static int udp_bind(struct protocol *protocol, struct socket *sock, const struct sockaddr *addr, socklen_t len)
{
    struct udp_protocol *udp = container_of(protocol, struct udp_protocol, proto);
    const struct sockaddr_in *in = (const struct sockaddr_in *)addr;
    int ret;

    if (len < sizeof(*in))
        return -EFAULT;

    if (addr->sa_family != AF_INET)
        return -EINVAL;

    sock->af_private.ipv4.src_addr = in->sin_addr.s_addr;
    sock->af_private.ipv4.src_port = in->sin_port;

    ret = udp_register_sock(udp, sock);
    if (ret) {
        sock->af_private.ipv4.src_port = htons(0);
        return ret;
    }

    return 0;
}

static int udp_autobind(struct protocol *protocol, struct socket *sock)
{
    int ret;
    struct udp_protocol *udp = container_of(protocol, struct udp_protocol, proto);

    sock->af_private.ipv4.src_addr = INADDR_ANY;
    sock->af_private.ipv4.src_port = udp_find_port(udp);

    ret = udp_register_sock(udp, sock);
    if (ret) {
        sock->af_private.ipv4.src_port = htons(0);
        return ret;
    }

    return 0;
}

static int udp_connect(struct protocol *protocol, struct socket *socket, const struct sockaddr *addr, socklen_t len)
{
    const struct sockaddr_in *in = (const struct sockaddr_in *)addr;

    if (len < sizeof(*in))
        return -EFAULT;

    if (addr->sa_family != AF_INET)
        return -EINVAL;

    socket->af_private.ipv4.dest_addr = in->sin_addr.s_addr;
    socket->af_private.ipv4.dest_port = in->sin_port;

    flag_set(&socket->flags, SOCKET_IS_CONNECTED);

    return 0;
}

static int udp_create(struct protocol *protocol, struct socket *sock)
{
    return 0;
}

static int udp_delete(struct protocol *protocol, struct socket *sock)
{
    struct address_family_ip *af = container_of(sock->af, struct address_family_ip, af);

    if (n32_nonzero(sock->af_private.ipv4.src_port)) {
        using_mutex(&af->lock)
            __ipaf_remove_socket(af, sock);
    }

    return 0;
}

static int udp_getsockname(struct protocol *protocol, struct socket *sock, struct sockaddr *addr, socklen_t *len)
{
    struct sockaddr_in *in = (struct sockaddr_in *)addr;

    if (*len < sizeof(*in))
        return -EFAULT;

    in->sin_port = sock->af_private.ipv4.src_port;
    *len = sizeof(*in);

    return 0;
}

void udp_lookup_fill(struct ip_lookup *lookup, struct packet *packet)
{
    struct udp_header *header = packet->head;

    lookup->src_port = header->dest_port;
    lookup->dest_port = header->source_port;
}

static struct protocol_ops udp_protocol_ops = {
    .packet_rx = udp_rx,
    .sendto = udp_sendto,
    .connect = udp_connect,
    .bind = udp_bind,
    .autobind = udp_autobind,
    .getsockname = udp_getsockname,

    .create = udp_create,
    .delete = udp_delete,
};

struct protocol *udp_get_proto(void)
{
    return &udp_protocol.proto;
}
