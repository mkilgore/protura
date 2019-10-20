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

static int tcp_checksum_valid(struct packet *packet)
{
    struct ip_header *ip_head = packet->af_head;
    struct pseudo_header pseudo_header;

    memset(&pseudo_header, 0, sizeof(pseudo_header));

    pseudo_header.saddr = ip_head->source_ip;
    pseudo_header.daddr = ip_head->dest_ip;
    pseudo_header.proto = ip_head->protocol;
    pseudo_header.len = htons(packet_len(packet));

    n16 checksum = tcp_checksum(&pseudo_header, packet->head, packet_len(packet));

    kp_tcp("Checksum result: %04x, plen: %d\n", ntohs(checksum), packet_len(packet));
    return ntohs(checksum) == 0;
}

void tcp_closed(struct protocol *proto, struct packet *packet)
{
    kp_tcp("tcp_closed()\n");
    /* FIXME: We need to send a RST even with no socket */
}

void tcp_syn_sent(struct protocol *proto, struct socket *sock, struct packet *packet)
{
    struct tcp_socket_private *priv = &sock->proto_private.tcp;
    struct tcp_packet_cb *seg = &packet->cb.tcp;

    kp_tcp("tcp_syn_sent()\n");

    /* first: check ACK bit */
    if (seg->flags.ack) {
        kp_tcp("seq: %u, ack_seq: %u, snd_nxt: %u, snd_una: %u\n", seg->seq, seg->ack_seq, priv->snd_nxt, priv->snd_una);
        if (seg->ack_seq <= priv->iss
            || seg->ack_seq > priv->snd_nxt
            || seg->ack_seq < priv->snd_una) {
            /* FiXME: Send reset */
            kp_tcp("tcp_syn_sent() - bad ACK, RST\n");
            goto release_packet;
        }
    }

    /* second: check RST bit */
    if (seg->flags.rst) {
        kp_tcp("tcp_syn_sent() - RST\n");
        priv->tcp_state = TCP_CLOSE;
        socket_set_last_error(sock, -ECONNREFUSED);
        socket_state_change(sock, SOCKET_UNCONNECTED);
        goto release_packet;
    }

    /* third: check security/precedence - ignored */

    if (!seg->flags.syn) {
        /* fifth: if SYN not set, drop packet */
        kp_tcp("tcp_syn_sent() - not SYN\n");
        goto release_packet;
    }

    /* forth: SYN bit is set */
    priv->rcv_nxt = seg->seq + 1;
    priv->irs = seg->seq;

    if (seg->flags.ack)
        priv->snd_una = seg->ack_seq;

    if (priv->snd_una > priv->iss) {
        priv->snd_una = priv->snd_nxt;

        kp_tcp("tcp_syn_sent() - SYN ACK, established! isr: %u, rcv_nxt: %u\n", priv->irs, priv->rcv_nxt);
        tcp_send_ack(proto, sock);
        priv->tcp_state = TCP_ESTABLISHED;

        socket_state_change(sock, SOCKET_CONNECTED);
    } else {
        priv->tcp_state = TCP_SYN_RECV;
        priv->snd_una = priv->iss;
        // tcp_send_synack(proto, sock);
    }

  release_packet:
    packet_free(packet);
    return;
}

static void tcp_listen(struct protocol *proto, struct socket *sock, struct packet *packet)
{
    /* FIXME: We don't yet support listen */
    packet_free(packet);
}

static int tcp_sequence_valid(struct socket *sock, struct packet *packet)
{
    struct tcp_packet_cb *seg = &packet->cb.tcp;
    struct tcp_socket_private *priv = &sock->proto_private.tcp;

    uint32_t seg_length = packet_len(packet);

    /* There are four cases, for each of seg_length and rcv_wnd being zero or nonzero. */
    if (!seg_length && !priv->rcv_wnd)
        if (seg->seq == priv->rcv_nxt)
            return 1;

    if (!seg_length && priv->rcv_wnd)
        if (tcp_seq_between(priv->rcv_nxt + 1, seg->seq, priv->rcv_nxt + priv->rcv_wnd))
            return 1;

    /* If a non-zero length, then verify that part of the packet is within the
     * rcv_wnd and is past rcv_nxt */
    if (seg_length && priv->rcv_wnd)
        if (tcp_seq_between(priv->rcv_nxt + 1, seg->seq, priv->rcv_nxt + priv->rcv_wnd)
            || tcp_seq_between(priv->rcv_nxt + 1, seg->seq + seg_length + 1, priv->rcv_nxt + priv->rcv_wnd))
            return 1;

    /* seg_length > 0 && priv->rcv_wnd == 0 is always invalid, so we don't check it. */
    return 0;
}

void tcp_packet_fill_cb(struct packet *packet)
{
    struct tcp_header *seg = packet->proto_head;

    packet->cb.tcp.seq = ntohl(seg->seq);
    packet->cb.tcp.ack_seq = ntohl(seg->ack_seq);
    packet->cb.tcp.window = ntohs(seg->window);
    packet->cb.tcp.flags = seg->flags;
}

/*
 * Pretty literal translation of the "Segment Arrives" section of RFC793
 */
void tcp_rx(struct protocol *proto, struct socket *sock, struct packet *packet)
{
    struct tcp_header *header = packet->head;

    /* If checksum is invalid, ignore */
    if (!tcp_checksum_valid(packet)) {
        kp_tcp("packet: %d -> %d, %d bytes, invalid checksum!\n", ntohs(header->source), ntohs(header->dest), packet_len(packet));
        return;
    }

    packet->proto_head = header;
    packet->head += header->hl * 4;

    tcp_packet_fill_cb(packet);
    struct tcp_packet_cb *seg = &packet->cb.tcp;

    kp_tcp("%d -> %d, %d bytes, valid checksum!\n", ntohs(header->source), ntohs(header->dest), packet_len(packet));
    kp_tcp("seq: %u, ack_seq: %u, flags: %d\n", seg->seq, seg->ack_seq, seg->flags.flags);

    if (!sock)
        return tcp_closed(proto, packet);

    using_socket_priv(sock) {
        struct tcp_socket_private *priv = &sock->proto_private.tcp;

        switch (priv->tcp_state) {
        case TCP_CLOSE:
            return tcp_closed(proto, packet);

        case TCP_SYN_SENT:
            return tcp_syn_sent(proto, sock, packet);

        case TCP_LISTEN:
            return tcp_listen(proto, sock, packet);
        }

        /* first: check sequence number */
        if (!tcp_sequence_valid(sock, packet)) {
            kp_tcp("packet sequence not valid, seq: %u, ack: %u, rcv_wnd: %u, rcv_nxt: %u\n", seg->seq, seg->ack_seq, priv->rcv_wnd, priv->rcv_nxt);

            /* If we get here, then the packet is not valid. We should send an ACK
             * unless we've been sent a RST, and then ignore the packet */
            if (!seg->flags.rst)
                tcp_send_ack(proto, sock);

            goto drop_packet;
        }

        /* second: check RST bit */
        if (seg->flags.rst) {
            kp_tcp("RST packet\n");
            /* In some cases, we set an error.
             * In all cases, we close the socket and drop the current packet */
            switch (priv->tcp_state) {
            case TCP_SYN_RECV:
                socket_set_last_error(sock, -ECONNREFUSED);

            case TCP_ESTABLISHED:
            case TCP_FIN_WAIT1:
            case TCP_FIN_WAIT2:
            case TCP_CLOSE_WAIT:
                socket_set_last_error(sock, -ECONNRESET);
            }

            priv->tcp_state = TCP_CLOSE;
            socket_state_change(sock, SOCKET_UNCONNECTED);
            goto drop_packet;
        }

        /* third: check security and precedence - ignored */

        /* forth: check SYN bit */
        if (seg->flags.syn) {
            kp_tcp("SYN packet\n");
            socket_set_last_error(sock, -ECONNRESET);
            priv->tcp_state = TCP_CLOSE;
            socket_state_change(sock, SOCKET_UNCONNECTED);
            goto drop_packet;
        }

        /* fifth: check ACK bit */
        if (!seg->flags.ack)
            goto drop_packet;

        kp_tcp("ACK packet\n");

        switch (priv->tcp_state) {
        case TCP_SYN_RECV:
            if (tcp_seq_between(priv->snd_una, seg->ack_seq, priv->snd_nxt + 1)) {
                priv->tcp_state = TCP_ESTABLISHED;
                socket_state_change(sock, SOCKET_CONNECTED);
            } else {
                /* FIXME: send RST, SEQ=seg->ack_seq */
                goto drop_packet;
            }
            break;

        case TCP_ESTABLISHED:
        case TCP_FIN_WAIT1:
        case TCP_FIN_WAIT2:
        case TCP_CLOSE_WAIT:
        case TCP_CLOSING:
        case TCP_LAST_ACK:
            if (tcp_seq_between(priv->snd_una, seg->ack_seq, priv->snd_nxt + 1)) {
                priv->snd_una = seg->ack_seq;
                /* FIXME: remove packets from retransmit queue */
            }

            if (tcp_seq_before(seg->ack_seq, priv->snd_una)) {
                /* already acked, ignore */
                kp_tcp("Packet already acked, ignoring ack information, ack_seq: %u, snd_una: %u\n", seg->ack_seq, priv->snd_una);
            }

            if (tcp_seq_after(seg->ack_seq, priv->snd_nxt)) {
                /* data past our next expected packet */
                goto drop_packet;
            }

            if (tcp_seq_between(priv->snd_una, seg->ack_seq, priv->snd_nxt + 1)) {
                if (tcp_seq_before(priv->snd_wl1, seg->seq)
                    || (priv->snd_wl1 == seg->seq
                        && tcp_seq_before(priv->snd_wl2 + 1, seg->ack_seq))) {
                    priv->snd_wnd = seg->window;
                    priv->snd_wl1 = seg->seq;
                    priv->snd_wl2 = seg->ack_seq;
                }
            }
        }

        /* extra ACK processing... */
        switch (priv->tcp_state) {
        case TCP_FIN_WAIT1:
            /* FIXME: check for FIN ack */
            break;

        case TCP_FIN_WAIT2:
            /* FIXME: ack user close */
            break;

        case TCP_CLOSING:
            /* FIXME: if acked our FIN, goto TCP_TIME_WAIT */
            goto drop_packet;

        case TCP_LAST_ACK:
            /* FIXME: If acked our FIN, close */
            break;

        case TCP_TIME_WAIT:
            /* FIXME: if FIN again, ack FIN and restart timer */
            break;
        }

        /* FIXME: sixth: check URG bit */

        /* cache the fin information, since the packet may be consumed when processing the data */
        int fin = seg->flags.fin;
        int fin_ack = seg->seq + packet_len(packet) + 1;

        /* seventh: process segment text */
        switch (priv->tcp_state) {
        case TCP_ESTABLISHED:
        case TCP_FIN_WAIT1:
        case TCP_FIN_WAIT2:
            if (seg->flags.psh || packet_len(packet)) {
                kp_tcp("recv data!\n");
                tcp_recv_data(proto, sock, packet);
                /* We still need to process FIN, but we cannot use the packet anymore */
                packet = NULL;
            }
            break;
        }

        if (!fin)
            goto drop_packet;

        switch (priv->tcp_state) {
        case TCP_SYN_RECV:
        case TCP_ESTABLISHED:
            priv->tcp_state = TCP_CLOSE_WAIT;
            break;

        case TCP_FIN_WAIT1:
            /* FIXME: has our FIN been acked? */
            break;

        case TCP_FIN_WAIT2:
            priv->tcp_state = TCP_TIME_WAIT;
            /* FIXME: start time-wait timer */
            break;

        case TCP_TIME_WAIT:
            /* FIXME: restart time-wait timer */
            break;
        }
    }

  drop_packet:
    if (packet)
        packet_free(packet);
}
