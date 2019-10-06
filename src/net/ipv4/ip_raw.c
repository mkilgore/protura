/*
 * Copyright (C) 2019 Matt Kilgore
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
#include <protura/net/proto.h>
#include <protura/net/netdevice.h>
#include <protura/net/arphrd.h>
#include <protura/net/ipv4/ipv4.h>
#include <protura/net/ipv4/ip_route.h>
#include <protura/net/linklayer.h>
#include <protura/net.h>
#include "ipv4.h"

static struct protocol_ops ip_raw_protocol_ops;

static struct protocol ip_raw_proto = PROTOCOL_INIT(&ip_raw_protocol_ops);

static void ip_raw_rx(struct protocol *proto, struct socket *sock, struct packet *packet)
{
    using_mutex(&sock->recv_lock) {
        list_add_tail(&sock->recv_queue, &packet->packet_entry);
        wait_queue_wake(&sock->recv_wait_queue);
    }
}

static int ip_raw_tx(struct protocol *proto, struct packet *packet)
{
    return ip_tx(packet);
}

static int ip_raw_sendto(struct protocol *proto, struct socket *sock, struct packet *packet, const struct sockaddr *addr, socklen_t len)
{
    const struct sockaddr_in *in = (const struct sockaddr_in *)addr;
    kp_ip("ip_raw_sendto: Sock: %p, Packet: %p, Dest: "PRin_addr"\n", sock, packet, Pin_addr(in->sin_addr.s_addr));

    ip_process_sockaddr(sock, packet, addr, len);
    packet->protocol_type = sock->protocol;
    return ip_tx(packet);
}

static int ip_raw_bind(struct protocol *proto, struct socket *sock, const struct sockaddr *addr, socklen_t len)
{
    const struct sockaddr_in *in = (const struct sockaddr_in *)addr;

    if (sizeof(*in) > len)
        return -EFAULT;

    sock->af_private.ipv4.src_addr = in->sin_addr.s_addr;
    return 0;
}

static int ip_raw_autobind(struct protocol *proto, struct socket *sock)
{
    sock->af_private.ipv4.src_addr = htonl(0);
    return 0;
}

static int ip_raw_getsockname(struct protocol *proto, struct socket *sock, struct sockaddr *addr, socklen_t *len)
{
    struct sockaddr_in *in = (struct sockaddr_in *)addr;

    if (*len < sizeof(*in))
        return -EFAULT;

    in->sin_addr.s_addr = sock->af_private.ipv4.src_addr;
    *len = sizeof(*in);

    return 0;
}

static int ip_raw_create(struct protocol *proto, struct socket *sock)
{
    struct address_family_ip *af = container_of(sock->af, struct address_family_ip, af);

    using_mutex(&af->lock)
        list_add_tail(&af->raw_sockets, &sock->socket_entry);

    return 0;
}

static int ip_raw_delete(struct protocol *proto, struct socket *sock)
{
    struct address_family_ip *af = container_of(sock->af, struct address_family_ip, af);

    using_mutex(&af->lock)
        list_del(&sock->socket_entry);

    return 0;
}

static struct protocol_ops ip_raw_protocol_ops  = {
    .packet_rx = ip_raw_rx,
    .packet_tx = ip_raw_tx,
    .sendto = ip_raw_sendto,

    .create = ip_raw_create,
    .delete = ip_raw_delete,

    .bind = ip_raw_bind,
    .autobind = ip_raw_autobind,
    .getsockname = ip_raw_getsockname,
};

struct protocol *ip_raw_get_proto(void)
{
    return &ip_raw_proto;
}
