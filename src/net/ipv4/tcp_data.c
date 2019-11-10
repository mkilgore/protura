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

#define TCP_DELACK_TIMER_MS 100

/* out-of-order packets should be added to the queue in order of their segment */
static void add_to_ooo_queue(struct socket *sock, struct packet *packet)
{
    struct tcp_packet_cb *seg = &packet->cb.tcp;

    struct packet *cur, *tmp;

    kp(KP_NORMAL, "Adding packet to OOO queue...\n");

    /* FIXME: It may be unlikely, but segments could contain overlapping data,
     * and we should make sure we don't duplicate that data back to the caller */
    list_foreach_entry_safe(&sock->out_of_order_queue, cur, tmp, packet_entry) {
        struct tcp_packet_cb *cur_seg = &cur->cb.tcp;

        if (tcp_seq_before(seg->seq, cur_seg->seq)) {
            kp(KP_NORMAL, "Adding packet %u before packet %u\n", seg->seq, cur_seg->seq);

            /* list_add_tail places it before the current segment */
            list_add_tail(&cur->packet_entry, &packet->packet_entry);
            return;
        }
    }

    kp(KP_NORMAL, "Adding packet %u at the end!\n", seg->seq);
    /* The packet is past every entry in the queue */
    list_add_tail(&sock->out_of_order_queue, &packet->packet_entry);
}

static void socket_recv_packet(struct socket *sock, struct packet *packet)
{
    struct tcp_socket_private *priv = &sock->proto_private.tcp;
    struct tcp_packet_cb *cur_seg = &packet->cb.tcp;

    priv->rcv_nxt += packet_len(packet);

    if (cur_seg->flags.fin) {
        priv->rcv_nxt++;
        tcp_fin(sock, packet);
    }

    kp_tcp("RECV packet seq: %u, len: %d, new rcv_nxt: %u\n", cur_seg->seq, packet_len(packet), priv->rcv_nxt);
    list_add_tail(&sock->recv_queue, &packet->packet_entry);
}

/* Checks the out-of-order queue and adds any pending packets to the recv queue
 * if they come next. */
static void consolidate_ooo_queue(struct socket *sock)
{
    struct tcp_socket_private *priv = &sock->proto_private.tcp;
    struct packet *cur, *tmp;

    kp_tcp("OOO consolidation...\n");
    list_foreach_entry_safe(&sock->out_of_order_queue, cur, tmp, packet_entry) {
        struct tcp_packet_cb *cur_seg = &cur->cb.tcp;

        kp_tcp("OOO packet seq: %u, rcv_nxt: %u...\n", cur_seg->seq, priv->rcv_nxt);
        if (cur_seg->seq == priv->rcv_nxt) {
            list_del(&cur->packet_entry);
            socket_recv_packet(sock, cur);
        }
    }
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

        using_mutex(&sock->recv_lock) {
            socket_recv_packet(sock, packet);
            consolidate_ooo_queue(sock);

            wait_queue_wake(&sock->recv_wait_queue);
        }

        tcp_delack_timer_start(sock, TCP_DELACK_TIMER_MS);
    } else {
        /* 
         * packet is in window, but not the next one in line.
         * add it to the out-of-order queue
         */

        using_mutex(&sock->recv_lock)
            add_to_ooo_queue(sock, packet);

        /* send a dup ack when we get an OOO packet */
        tcp_send_ack(proto, sock);
    }
}
