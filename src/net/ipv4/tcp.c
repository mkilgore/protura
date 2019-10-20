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
#include <protura/wait.h>
#include <protura/snprintf.h>
#include <protura/list.h>
#include <arch/asm.h>

#include <protura/net/socket.h>
#include <protura/net/ipv4/tcp.h>
#include <protura/net/ipv4/ipv4.h>
#include <protura/net.h>
#include "ipv4.h"
#include "tcp.h"

#define TCP_LOWEST_AUTOBIND_PORT 50000

struct tcp_protocol {
    struct protocol proto;

    mutex_t lock;
    uint16_t next_port;
};

static struct protocol_ops tcp_protocol_ops;

static struct tcp_protocol tcp_protocol = {
    .proto = PROTOCOL_INIT("tcp", tcp_protocol.proto, &tcp_protocol_ops),
    .lock = MUTEX_INIT(tcp_protocol.lock, "tcp-protocol-lock"),
    .next_port = TCP_LOWEST_AUTOBIND_PORT,
};

static uint32_t sum_every_16(const void *data, size_t len)
{
    uint32_t sum = 0;
    const uint16_t *ptr = data;

    for (; len > 1; len -= 2, ptr++)
        sum += *ptr;

    if (len > 0)
        sum += *(uint8_t *)ptr;

    return sum;
}

n16 tcp_checksum(struct pseudo_header *header, const char *data, size_t len)
{
    uint32_t sum = 0;

    sum = sum_every_16(header, sizeof(*header));
    sum += sum_every_16(data, len);

    /* Fold bits over to get the one's complement sum */
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return n16_make(~sum);
}

n16 tcp_checksum_packet(struct packet *packet)
{
    struct pseudo_header psuedo;
    struct sockaddr_in *in = (struct sockaddr_in *)&packet->dest_addr;

    memset(&psuedo, 0, sizeof(psuedo));

    using_netdev_read(packet->iface_tx)
        psuedo.saddr = packet->iface_tx->in_addr;

    psuedo.daddr = in->sin_addr.s_addr;
    psuedo.zero = 0;
    psuedo.proto = IPPROTO_TCP;
    psuedo.len = htons(packet_len(packet));

    return tcp_checksum(&psuedo, packet->head, packet_len(packet));
}

static int tcp_autobind(struct protocol *proto, struct socket *sock)
{
    struct tcp_protocol *tcp = container_of(proto, struct tcp_protocol, proto);
    int port = 0;

    /* FIXME: We need to verify if this port is free */
    using_mutex(&tcp->lock);
        port = tcp->next_port++;

    sock->af_private.ipv4.src_port = htons(port);

    return 0;
}

void tcp_lookup_fill(struct ip_lookup *lookup, struct packet *packet)
{
    struct tcp_header *tcp_head = packet->head;

    lookup->src_port = tcp_head->dest;
    lookup->dest_port = tcp_head->source;
}

static int tcp_connect(struct protocol *proto, struct socket *sock, const struct sockaddr *addr, socklen_t len)
{
    struct address_family_ip *af = container_of(sock->af, struct address_family_ip, af);
    struct tcp_socket_private *priv = &sock->proto_private.tcp;
    struct ipv4_socket_private *ip_priv = &sock->af_private.ipv4;
    int ret = 0;

    const struct sockaddr_in *in = (const struct sockaddr_in *)addr;

    kp_tcp("TCP connect...\n");

    if (len < sizeof(*in))
        return -EFAULT;

    if (addr->sa_family != AF_INET)
        return -EINVAL;

    enum socket_state cur_state = socket_state_cmpxchg(sock, SOCKET_UNCONNECTED, SOCKET_CONNECTING);
    if (cur_state != SOCKET_UNCONNECTED)
        return -EISCONN;

    using_socket_priv(sock) {
        ip_priv->dest_addr = in->sin_addr.s_addr;
        ip_priv->dest_port = in->sin_port;

        ret = ip_route_get(in->sin_addr.s_addr, &ip_priv->route);
        if (ret)
            return ret;

        using_netdev_read(ip_priv->route.iface)
            ip_priv->src_addr = ip_priv->route.iface->in_addr;

        if (n32_equal(ip_priv->src_port, htons(0)))
            tcp_autobind(proto, sock);

        struct ip_lookup test_lookup = {
            .proto = IPPROTO_TCP,
            .src_port = ip_priv->src_port,
            .src_addr = ip_priv->src_addr,
            .dest_port = ip_priv->dest_port,
            .dest_addr = ip_priv->dest_addr,
        };

        using_mutex(&af->lock) {
            struct socket *s = __ipaf_find_socket(af, &test_lookup, 4);
            if (s) {
                ret = -EADDRINUSE;
                break;
            }

            kp_udp("Adding tcp socket, src_port: %d, src_addr: "PRin_addr", dest_port: %d, dest_addr: "PRin_addr"\n", ntohs(test_lookup.src_port), Pin_addr(test_lookup.src_addr), ntohs(test_lookup.dest_port), Pin_addr(test_lookup.dest_addr));

            __ipaf_add_socket(af, sock);
        }

        if (ret)
            return ret;

        socket_state_change(sock, SOCKET_CONNECTING);

        priv->rcv_wnd = 44477;

        priv->iss = 200;
        priv->snd_una = 200;
        priv->snd_up = 200;
        priv->snd_nxt = 200;

        priv->snd_wnd= 0;
        priv->snd_wl1 = 0;
        priv->rcv_nxt = 0;

        kp_tcp("TCP connect sending SYN packet...\n");
        tcp_send_syn(proto, sock);
        priv->snd_nxt++;
    }

    int last_err;
    ret = wait_queue_event_intr(&sock->state_changed, ({
        cur_state = socket_state_get(sock);
        last_err = socket_last_error(sock);

        last_err || cur_state != SOCKET_CONNECTING;
    }));

    if (ret)
        return ret;

    kp(KP_NORMAL, "Socket: got state_changed signal current state: %d, last_err: %d\n", cur_state, last_err);

    if (last_err || cur_state != SOCKET_CONNECTED)
        return last_err;

    return 0;
}

static int tcp_create(struct protocol *proto, struct socket *sock)
{
    tcp_socket_private_init(&sock->proto_private.tcp);
    tcp_timers_init(sock);
    return 0;
}

static int tcp_delete(struct protocol *proto, struct socket *sock)
{
    struct address_family_ip *af = container_of(sock->af, struct address_family_ip, af);

    using_mutex(&af->lock)
        __ipaf_remove_socket(af, sock);

    tcp_timers_reset(sock);
    return 0;
}

static struct protocol_ops tcp_protocol_ops = {
    .packet_rx = tcp_rx,
    .autobind = tcp_autobind,

    .create = tcp_create,
    .delete = tcp_delete,

    .connect = tcp_connect,
};

struct protocol *tcp_get_proto(void)
{
    return &tcp_protocol.proto;
}
