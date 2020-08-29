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

static void tcp_send_inner(struct protocol *proto, struct packet *packet)
{
    struct tcp_packet_cb *cb = &packet->cb.tcp;
    struct tcp_header *head;

    packet->head -= sizeof(*head);
    head = packet->head;
    packet->proto_head = head;

    memset(head, 0, sizeof(*head));

    head->hl = sizeof(*head) / 4;

    head->source = sockaddr_in_get_port(&packet->src_addr);
    head->dest = sockaddr_in_get_port(&packet->dest_addr);

    head->flags = cb->flags;

    head->seq = htonl(cb->seq);
    head->ack_seq = htonl(cb->ack_seq);
    head->window = htons(cb->window);
    head->urg_ptr = htons(0);

    kp_tcp_trace("TCP send packet, flags: 0x%02x, seq: %u, ack_seq: %u, len: %d\n", cb->flags.flags, cb->seq, cb->ack_seq, packet_len(packet));

    packet->protocol_type = IPPROTO_TCP;

    head->check = htons(0);

    head->check = tcp_checksum_packet(packet);
    ip_tx(packet);
}

void tcp_send_raw(struct protocol *proto, struct packet *packet, n16 src_port, n32 dest_addr, n16 dest_port)
{
    int ret = ip_packet_fill_raw(packet, dest_addr);
    kp_tcp_trace("TCP send packet, ip route ret: %d\n", ret);
    if (ret) {
        packet_free(packet);
        return;
    }

    sockaddr_in_assign_port(&packet->src_addr, src_port);
    sockaddr_in_assign_port(&packet->dest_addr, dest_port);

    tcp_send_inner(proto, packet);
}

void tcp_send(struct protocol *proto, struct socket *sock, struct packet *packet)
{
    struct ipv4_socket_private *ip_priv = &sock->af_private.ipv4;

    int ret = ip_packet_fill_route(sock, packet);
    kp_tcp_trace("TCP send packet, ip route ret: %d\n", ret);
    if (ret) {
        packet_free(packet);
        return;
    }

    sockaddr_in_assign_port(&packet->src_addr, ip_priv->src_port);
    sockaddr_in_assign_port(&packet->dest_addr, ip_priv->dest_port);

    packet->sock = socket_dup(sock);
    tcp_send_inner(proto, packet);
}

void tcp_send_syn(struct protocol *proto, struct socket *sock)
{
    struct tcp_socket_private *priv = &sock->proto_private.tcp;
    struct packet *packet = packet_new(PAL_KERNEL);
    struct tcp_packet_cb *cb = &packet->cb.tcp;

    cb->seq = priv->snd_nxt;
    cb->ack_seq = priv->rcv_nxt;
    cb->window = priv->rcv_wnd;
    cb->flags.syn = 1;

    tcp_send(proto, sock, packet);

    sock->proto_private.tcp.tcp_state = TCP_SYN_SENT;
}

void tcp_send_ack(struct protocol *proto, struct socket *sock)
{
    struct tcp_socket_private *priv = &sock->proto_private.tcp;
    struct packet *packet = packet_new(PAL_KERNEL);
    struct tcp_packet_cb *cb = &packet->cb.tcp;

    cb->seq = priv->snd_nxt;
    cb->ack_seq = priv->rcv_nxt;
    cb->window = priv->rcv_wnd;
    cb->flags.ack = 1;

    tcp_delack_timer_stop(sock);
    tcp_send(proto, sock, packet);
}

void tcp_send_reset(struct protocol *proto, struct packet *old_packet)
{
    struct tcp_packet_cb *old_cb = &old_packet->cb.tcp;

    struct tcp_header *tcp_head = old_packet->proto_head;
    struct ip_header *ip_head = old_packet->af_head;

    struct packet *packet = packet_new(PAL_KERNEL);
    struct tcp_packet_cb *cb = &packet->cb.tcp;

    /* If the received packet is an RST, don't send one in response */
    if (old_cb->flags.rst)
        goto release_old_packet;

    /* If we're responding to an ACK, we make it look like an acceptable ACK
     * If not, we just use sequence number 0 */
    if (!old_cb->flags.ack) {
        cb->seq = 0;
        cb->ack_seq = old_cb->seq + packet_len(old_packet);
        cb->flags.rst = 1;
        cb->flags.ack = 1;

        if (old_cb->flags.syn)
            cb->ack_seq++;
    } else {
        cb->seq = old_cb->ack_seq;
        cb->flags.rst = 1;
    }

    kp_tcp_trace("RESET, SrcIP: "PRin_addr", DestIP: "PRin_addr"\n", Pin_addr(ip_head->source_ip), Pin_addr(ip_head->dest_ip));
    tcp_send_raw(proto, packet, tcp_head->dest, ip_head->source_ip, tcp_head->source);

  release_old_packet:
    /* Consume the old packet */
    packet_free(old_packet);
}
