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

void __ipaf_add_socket(struct address_family_ip *af, struct socket *sock)
{
    socket_dup(sock);
    list_add_tail(&af->sockets, &sock->socket_entry);
}

void __ipaf_remove_socket(struct address_family_ip *af, struct socket *sock)
{
    list_del(&sock->socket_entry);
    socket_put(sock);
}

__must_check
struct socket *__ipaf_find_socket(struct address_family_ip *af, struct ip_lookup *addr, int total_max_score)
{
    struct socket *sock;
    struct socket *ret = NULL;
    int maxscore = 0;

    /* This looks complicated, but is actually pretty simple
     * When an IP packet comes in, we have to amtch it to a coresponding
     * socket, which is marked with a protocl, source/dest port, and
     * source/dest IP addr.
     *
     * In the case of a listening socket, those source/dest values may be 0,
     * representing a bind to *any* value, so we skip checking those.
     * Otherwise, the values have to matche exactly what we were passed.
     *
     * Beyond that, if we have, say, a socket listening for INADDR_ANY (zero)
     * on port X, and a socket with a direct connection to some specific IP on
     * port X, we want to send the packet to the direct connection and not to
     * the listening socket. To acheive that, we keep a "score" of how many
     * details of the conneciton matched, and then select the socket with the
     * highest score at the end (4 is the highest score possible, so we return
     * right away if we hit that).
     */
    list_foreach_entry(&af->sockets, sock, socket_entry) {
        struct ipv4_socket_private *sockroute = &sock->af_private.ipv4;
        int score = 0;

        if (sockroute->proto != addr->proto)
            continue;

        if (n16_nonzero(sockroute->src_port)) {
            if (!n16_equal(sockroute->src_port, addr->src_port))
                continue;
            score++;
        }

        if (n32_nonzero(sockroute->src_addr)) {
            if (!n32_equal(sockroute->src_addr, addr->src_addr))
                continue;
            score++;
        }

        if (n16_nonzero(sockroute->dest_port)) {
            if (!n16_equal(sockroute->dest_port, addr->dest_port))
                continue;
            score++;
        }

        if (n32_nonzero(sockroute->dest_addr)) {
            if (!n32_equal(sockroute->dest_addr, addr->dest_addr))
                continue;
            score++;
        }

        if (score == total_max_score)
            return sock;

        if (maxscore >= score)
            continue;

        maxscore = score;
        ret = sock;
    }

    if (ret)
        ret = socket_dup(ret);

    return ret;
}

