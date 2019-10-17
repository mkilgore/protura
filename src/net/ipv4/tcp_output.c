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
#include <protura/net/ipv4/tcp.h>
#include <protura/net/ipv4/ipv4.h>
#include <protura/net.h>
#include "ipv4.h"
#include "tcp.h"

struct tcp_raw_args {
    struct protocol *proto;
    struct socket *sock;
    uint8_t flags;
    n32 seq;
    n32 seq_ack;
    n16 window;
};

// void tcp_send_raw(struct protocol *proto, struct socket *sock, struct packet *packet, 

void tcp_send(struct protocol *proto, struct socket *sock, struct packet *packet, uint8_t flags, n32 seq)
{
    struct tcp_socket_private *priv = &sock->proto_private.tcp;
    struct ipv4_socket_private *ip_priv = &sock->af_private.ipv4;

    kp_tcp("TCP send packet, flags: %d, seq: %d\n", flags, ntohl(seq));

    struct tcp_header *head;

    packet->head -= sizeof(*head);
    head = packet->head;
    packet->proto_head = head;

    memset(head, 0, sizeof(*head));

    head->hl = sizeof(*head) / 4;

    head->source = ip_priv->src_port;
    head->dest = ip_priv->dest_port;

    head->flags.flags = flags;

    head->seq = seq;
    head->ack_seq = htonl(priv->rcv_nxt);
    head->window = htons(priv->rcv_wnd);
    head->urg_ptr = htons(0);

    packet->protocol_type = IPPROTO_TCP;

    int ret = ip_packet_fill_route(sock, packet);
    kp_tcp("TCP send packet, ip route ret: %d\n", ret);
    if (ret) {
        packet_free(packet);
        return;
    }

    sockaddr_in_assign_port(&packet->src_addr, ip_priv->src_port);
    sockaddr_in_assign_port(&packet->dest_addr, ip_priv->dest_port);

    head->check = htons(0);

    head->check = tcp_checksum_packet(packet);
    if (!packet->sock)
        packet->sock = socket_dup(sock);

    ip_tx(packet);
}

void tcp_send_syn(struct protocol *proto, struct socket *sock)
{
    struct tcp_socket_private *priv = &sock->proto_private.tcp;
    struct packet *packet = packet_new(PAL_KERNEL);

    tcp_send(proto, sock, packet, TCP_SYN, htonl(priv->snd_nxt));

    sock->proto_private.tcp.tcp_state = TCP_SYN_SENT;
}

void tcp_send_ack(struct protocol *proto, struct socket *sock)
{
    struct tcp_socket_private *priv = &sock->proto_private.tcp;
    struct packet *packet = packet_new(PAL_KERNEL);

    tcp_send(proto, sock, packet, TCP_ACK, htonl(priv->snd_nxt));
}
