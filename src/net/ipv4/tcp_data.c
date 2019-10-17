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

/* out-of-order packets should be added to the queue in order of their segment */
static void add_to_ooo_queue(struct socket *sock, struct packet *packet)
{
    struct tcp_socket_private *priv = &sock->proto_private.tcp;
    struct tcp_header *seg = packet->proto_head;

    struct packet *cur, *tmp;

    // list_foreach_entry_safe(&sock->out_of_order_queue, cur, tmp, packet_entry) {
    //     struct tcp_header *cur_seg = cur->proto_head;
    // }
}

void tcp_recv_data(struct protocol *proto, struct socket *sock, struct packet *packet)
{
    struct tcp_socket_private *priv = &sock->proto_private.tcp;
    struct tcp_packet_cb *seg = &packet->cb.tcp;

    if (!priv->rcv_wnd) {
        packet_free(packet);
        return;
    }

    kp_tcp("seq: %u, rcv_nxt: %u\n", seg->seq, priv->rcv_nxt);
    if (seg->seq == priv->rcv_nxt) {
        priv->rcv_nxt += packet_len(packet);

        using_mutex(&sock->recv_lock) {
            list_add_tail(&sock->recv_queue, &packet->packet_entry);
            wait_queue_wake(&sock->recv_wait_queue);
        }

        /* FIXME: This should be piggy-packed onto another packet, via a timer */
        tcp_send_ack(proto, sock);
    } else {
        /* 
         * packet is in window, but not the next one in line.
         * add it to the out-of-order queue
         */

        /* FIXME: We're ignoring these packets and just sending an ACK */
        /* using_mutex(&sock->recv_lock)
               add_to_ooo_queue(sock, packet); */

        /* send a dup ack when we get an OOO packet */
        tcp_send_ack(proto, sock);
    }
}
