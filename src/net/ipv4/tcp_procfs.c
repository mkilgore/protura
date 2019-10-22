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

void tcp_procfs_register(struct protocol *proto, struct socket *sock)
{
    using_mutex(&proto->lock) {
        sock = socket_dup(sock);
        list_add_tail(&proto->socket_list, &sock->proto_entry);
    }
}

void tcp_procfs_unregister(struct protocol *proto, struct socket *sock)
{
    using_mutex(&proto->lock) {
        list_del(&sock->proto_entry);
        socket_put(sock);
    }
}

static int tcp_seq_start(struct seq_file *seq)
{
    return proto_seq_start(seq, &tcp_protocol.proto);
}

static const char *tcp_state_str[] = {
    [0]               = "NONE",
    [TCP_ESTABLISHED] = "ESTABLISHED",
    [TCP_SYN_SENT]    = "SYN-SENT",
    [TCP_SYN_RECV]    = "SYN-RECV",
    [TCP_FIN_WAIT1]   = "FIN-WAIT1",
    [TCP_FIN_WAIT2]   = "FIN-WAIT2",
    [TCP_TIME_WAIT]   = "TIME-WAIT",
    [TCP_CLOSE]       = "CLOSE",
    [TCP_CLOSE_WAIT]  = "CLOSE-WAIT",
    [TCP_LAST_ACK]    = "LAST-ACK",
    [TCP_LISTEN]      = "LISTEN",
    [TCP_CLOSING]     = "CLOSING",
};

static int tcp_seq_render(struct seq_file *seq)
{
    struct socket *s = proto_seq_get_socket(seq);
    if (!s)
        return seq_printf(seq, "LocalAddr\t"
                               "LocalPort\t"
                               "RemoteAddr\t"
                               "RemotePort\t"
                               "STATE\t"
                               "ISS\t"
                               "IRS\t"
                               "RCV.NXT\t"
                               "RCV.UP\t"
                               "RCV.WND\t"
                               "SND.NXT\t"
                               "SND.UNA\t"
                               "SND.UP\t"
                               "SND.WND\t"
                               "SND.WL1\t"
                               "SND.WL2\n"
                               );

    struct ipv4_socket_private *ip_priv = &s->af_private.ipv4;
    struct tcp_socket_private *tcp_priv = &s->proto_private.tcp;

    using_socket_priv(s)
        return seq_printf(seq, PRin_addr"\t"
                               "%d\t"
                               PRin_addr"\t"
                               "%d\t"
                               "%s\t"
                               "%u\t"
                               "%u\t"
                               "%u\t"
                               "%u\t"
                               "%u\t"
                               "%u\t"
                               "%u\t"
                               "%u\t"
                               "%u\t"
                               "%u\n",
                Pin_addr(ip_priv->src_addr),
                ntohs(ip_priv->src_port),
                Pin_addr(ip_priv->dest_addr),
                ntohs(ip_priv->dest_port),
                tcp_state_str[tcp_priv->tcp_state],
                tcp_priv->iss,
                tcp_priv->irs,
                tcp_priv->rcv_nxt,
                tcp_priv->rcv_up,
                tcp_priv->rcv_wnd,
                tcp_priv->snd_nxt,
                tcp_priv->snd_una,
                tcp_priv->snd_up,
                tcp_priv->snd_wnd,
                tcp_priv->snd_wl1,
                tcp_priv->snd_wl2
                );
}

static const struct seq_file_ops tcp_seq_file_ops = {
    .start = tcp_seq_start,
    .render = tcp_seq_render,
    .next = proto_seq_next,
    .end = proto_seq_end,
};

static int tcp_file_seq_open(struct inode *inode, struct file *filp)
{
    return seq_open(filp, &tcp_seq_file_ops);
}

struct file_ops tcp_proc_file_ops = {
    .open = tcp_file_seq_open,
    .lseek = seq_lseek,
    .read = seq_read,
    .release = seq_release,
};
