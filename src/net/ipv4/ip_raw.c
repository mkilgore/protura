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

static struct protocol ip_raw_proto = PROTOCOL_INIT("ip-raw", ip_raw_proto, &ip_raw_protocol_ops);

static void ip_raw_rx(struct protocol *proto, struct socket *sock, struct packet *packet)
{
    using_mutex(&sock->recv_lock) {
        list_add_tail(&sock->recv_queue, &packet->packet_entry);
        wait_queue_wake(&sock->recv_wait_queue);
    }
}

static int ip_raw_sendto(struct protocol *proto, struct socket *sock, struct user_buffer buf, size_t buf_len, const struct sockaddr *addr, socklen_t len)
{
    int err = socket_last_error(sock);
    if (err)
        return err;

    while (buf_len) {
        struct packet *packet = packet_new(PAL_KERNEL);
        size_t append_len = (buf_len > IPV4_PACKET_MSS)? IPV4_PACKET_MSS: buf_len;

        int err = packet_append_user_data(packet, buf, append_len);
        if (err) {
            packet_free(packet);
            return err;
        }

        buf_len -= append_len;
        buf = user_buffer_index(buf, append_len);

        packet->sock = socket_dup(sock);

        int ret = ip_packet_fill_route_addr(sock, packet, addr, len);
        if (ret) {
            packet_free(packet);
            return ret;
        }

        packet->protocol_type = sock->protocol;

        using_socket_priv(sock)
            ip_tx(packet);
    }

    return 0;
}

static int ip_raw_bind(struct protocol *proto, struct socket *sock, const struct sockaddr *addr, socklen_t len)
{
    const struct sockaddr_in *in = (const struct sockaddr_in *)addr;

    if (sizeof(*in) > len)
        return -EFAULT;

    using_socket_priv(sock)
        sock->af_private.ipv4.src_addr = in->sin_addr.s_addr;

    return 0;
}

static int ip_raw_autobind(struct protocol *proto, struct socket *sock)
{
    using_socket_priv(sock)
        sock->af_private.ipv4.src_addr = htonl(0);

    return 0;
}

static int ip_raw_getsockname(struct protocol *proto, struct socket *sock, struct sockaddr *addr, socklen_t *len)
{
    struct sockaddr_in *in = (struct sockaddr_in *)addr;

    if (*len < sizeof(*in))
        return -EFAULT;

    using_socket_priv(sock)
        in->sin_addr.s_addr = sock->af_private.ipv4.src_addr;

    *len = sizeof(*in);

    return 0;
}

static int ip_raw_create(struct protocol *proto, struct socket *sock)
{
    struct address_family_ip *af = container_of(sock->af, struct address_family_ip, af);

    using_mutex(&proto->lock) {
        sock = socket_dup(sock);
        list_add_tail(&proto->socket_list, &sock->proto_entry);
    }

    using_mutex(&af->lock) {
        sock = socket_dup(sock);
        list_add_tail(&af->raw_sockets, &sock->socket_entry);
    }

    return 0;
}

static int ip_raw_shutdown(struct protocol *proto, struct socket *sock, int how)
{
    return -ENOTCONN;
}

static void ip_raw_release(struct protocol *proto, struct socket *sock)
{
    struct address_family_ip *af = container_of(sock->af, struct address_family_ip, af);

    ip_release(sock->af, sock);

    using_mutex(&proto->lock) {
        list_del(&sock->proto_entry);
        socket_put(sock);
    }

    using_mutex(&af->lock) {
        list_del(&sock->socket_entry);
        socket_put(sock);
    }

    socket_state_change(sock, SOCKET_UNCONNECTED);
}

static struct protocol_ops ip_raw_protocol_ops  = {
    .packet_rx = ip_raw_rx,
    .sendto = ip_raw_sendto,

    .create = ip_raw_create,
    .release = ip_raw_release,

    .bind = ip_raw_bind,
    .autobind = ip_raw_autobind,
    .getsockname = ip_raw_getsockname,
    .shutdown = ip_raw_shutdown,
};

struct protocol *ip_raw_get_proto(void)
{
    return &ip_raw_proto;
}

static int ip_raw_seq_start(struct seq_file *seq)
{
    return proto_seq_start(seq, &ip_raw_proto);
}

static int ip_raw_seq_render(struct seq_file *seq)
{
    struct socket *s = proto_seq_get_socket(seq);
    if (!s)
        return seq_printf(seq, "LocalAddr RemoteAddr\n");

    struct ipv4_socket_private *private = &s->af_private.ipv4;

    return seq_printf(seq, PRin_addr" "PRin_addr"\n",
            Pin_addr(private->src_addr), Pin_addr(private->dest_addr));
}

static const struct seq_file_ops ip_raw_seq_file_ops = {
    .start = ip_raw_seq_start,
    .render = ip_raw_seq_render,
    .next = proto_seq_next,
    .end = proto_seq_end,
};

static int ip_raw_file_seq_open(struct inode *inode, struct file *filp)
{
    return seq_open(filp, &ip_raw_seq_file_ops);
}

struct file_ops ip_raw_proc_file_ops = {
    .open = ip_raw_file_seq_open,
    .lseek = seq_lseek,
    .read = seq_read,
    .release = seq_release,
};
