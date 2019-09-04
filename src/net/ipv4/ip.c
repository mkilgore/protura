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

static struct address_family_ops ip_address_family_ops;

struct address_family_ip ip_address_family = {
    .af = ADDRESS_FAMILY_INIT(ip_address_family.af, AF_INET, &ip_address_family_ops),
    .lock = MUTEX_INIT(ip_address_family.lock, "ip-address-family-lock"),
    .raw_sockets = LIST_HEAD_INIT(ip_address_family.raw_sockets),
    .sockets = LIST_HEAD_INIT(ip_address_family.sockets),
};

in_addr_t inet_addr(const char *ip)
{
    int vals[4];
    int i;

    for (i = 0; i < 4; i++) {
        int new_val = 0;
        while (*ip >= '0' && *ip <= '9') {
            new_val = new_val * 10 + (*ip - '0');
            ip++;
        }

        vals[i] = new_val;
        ip++;
    }

    return n32_make((vals[3] << 24) + (vals[2] << 16) + (vals[1] << 8) + vals[0]);
}

n16 ip_chksum(uint16_t *head, size_t byte_count)
{
    size_t i;
    uint32_t sum = 0;

    for (i = 0; i < byte_count / 2; i++)
        sum += head[i];

    if (byte_count % 2)
        sum += head[byte_count];

    while (sum & 0xFFFF0000)
        sum = (sum & 0xFFFF) + ((sum & 0xFFFF0000) >> 16);

    return n16_make(~sum);
}

static uint16_t next_ip_id = 0;

void ip_rx(struct address_family *afamily, struct packet *packet)
{
    struct address_family_ip *af = container_of(afamily, struct address_family_ip, af);
    struct ip_header *header = packet->head;
    struct sockaddr_in *in;
    int packet_handled = 0;

    packet->af_head = packet->head;
    packet->head += header->ihl * 4;
    packet->tail = packet->head + ntohs(header->total_length) - header->ihl * 4;

    kp_ip("  Packet: "PRin_addr" -> "PRin_addr"\n", Pin_addr(header->source_ip), Pin_addr(header->dest_ip));
    kp_ip("  af_head: %p, start: %p, offset: %ld\n", packet->af_head, packet->start, packet->af_head - packet->start);
    kp_ip("  Version: %d, HL: %d, %d bytes\n", header->version, header->ihl, header->ihl * 4);
    kp_ip("  Protocol: 0x%02x, ID: 0x%04x, Len: 0x%04x\n", header->protocol, ntohs(header->id), ntohs(header->total_length) - header->ihl * 4);
    kp_ip("  Checksum:   0x%04x\n", ntohs(header->csum));
    header->csum = htons(0);
    kp_ip("  Calculated: 0x%04x\n", ntohs(ip_chksum((uint16_t *)header, header->ihl * 4)));

    in = (struct sockaddr_in *)&packet->src_addr;

    in->sin_family = AF_INET;
    in->sin_addr.s_addr = header->source_ip;
    packet->src_len = sizeof(*in);

    /* First, route a copy to any raw sockets */
    using_mutex(&af->lock) {
        struct socket *raw;
        list_foreach_entry(&af->raw_sockets, raw, socket_entry) {
            if (raw->protocol == header->protocol) {
                struct packet *copy = packet_copy(packet, PAL_KERNEL);
                copy->sock = socket_dup(raw);

                (raw->proto->ops->packet_rx) (raw->proto, copy);
                packet_handled = 1;
            }
        }
    }

    struct socket *sock = NULL;
    struct ip_lookup lookup = {
        .proto = header->protocol,
        .src_addr = header->dest_ip,
        .dest_addr = header->source_ip,
    };

    int maxscore = 2;

    switch (header->protocol) {
    case IPPROTO_TCP:
        tcp_lookup_fill(&lookup, packet);
        maxscore = 4;
        break;

    case IPPROTO_UDP:
        udp_lookup_fill(&lookup, packet);
        maxscore = 4;
        break;
    }

    using_mutex(&af->lock) {
        sock = __ipaf_find_socket(af, &lookup, maxscore);
        if (sock)
            packet->sock = socket_dup(sock);
    }

    if (sock)
        (sock->proto->ops->packet_rx) (sock->proto, packet);
    else {
        if (!packet_handled)
            kp_ip("  Packet Dropped! %p\n", packet);
        packet_free(packet);
    }
}

int ip_tx(struct packet *packet)
{
    struct ip_route_entry route;
    struct ip_header *header;
    struct sockaddr_in *in = (struct sockaddr_in *)&packet->dest_addr;
    size_t data_len;

    data_len = packet_len(packet);

    packet->head -= sizeof(struct ip_header);
    header = packet->head;

    memset(header, 0, sizeof(*header));

    header->version = 4;
    header->ihl = 5;
    header->tos = 0;
    header->id = htons(next_ip_id++);
    header->frag_off = htons(0);
    header->ttl = 30;
    header->protocol = packet->protocol_type;
    header->total_length = htons(data_len + sizeof(*header));

    int ret = ip_route_get(in->sin_addr.s_addr, &route);
    if (ret)
        return -EACCES;

    packet->iface_tx = route.iface;
    packet->ll_type = htons(ETH_P_IP);

    using_netdev_read(route.iface)
        header->source_ip = route.iface->in_addr;

    packet->route_addr = ip_route_get_ip(&route);

    header->dest_ip = in->sin_addr.s_addr;
    header->csum = htons(0);
    header->csum = ip_chksum((uint16_t *)header, header->ihl * 4);

    kp_ip("Ip route: "PRin_addr", len: %d, csum: 0x%04x, DestIP: "PRin_addr"\n", Pin_addr(packet->route_addr), data_len, ntohs(header->csum), Pin_addr(header->dest_ip));

    return (packet->iface_tx->linklayer_tx) (packet);
}

int ip_process_sockaddr(struct packet *packet, const struct sockaddr *addr, socklen_t len)
{
    if (addr) {
        const struct sockaddr_in *in;

        if (sizeof(*in) > len)
            return -EFAULT;

        in = (const struct sockaddr_in *)addr;

        sockaddr_in_assign_addr(&packet->dest_addr, in->sin_addr.s_addr);
    } else if (flag_test(&packet->sock->flags, SOCKET_IS_BOUND)) {
        sockaddr_in_assign_addr(&packet->dest_addr, packet->sock->af_private.ipv4.src_addr);
    } else {
        return -EDESTADDRREQ;
    }

    return 0;
}

static int ip_create(struct address_family *family, struct socket *socket)
{
    if (socket->sock_type == SOCK_DGRAM
        && (socket->protocol == 0 || socket->protocol == IPPROTO_UDP)) {
        kp_ip("Looking up UDP protocol\n");

        socket->proto = protocol_lookup(PROTOCOL_UDP);
        socket->af_private.ipv4.proto = IPPROTO_UDP;
    } else if (socket->sock_type == SOCK_RAW) {
        if (socket->protocol > 255 || socket->protocol < 0)
            return -EINVAL;

        socket->proto = protocol_lookup(PROTOCOL_RAW);
        socket->af_private.ipv4.proto = socket->protocol;
        kp(KP_NORMAL, "Proto: %p\n", socket->proto);
    } else {
        return -EINVAL;
    }

    return 0;
}

static int ip_delete(struct address_family *family, struct socket *socket)
{
    // struct address_family_ip *ip = (struct address_family_ip *)family;

    // using_mutex(&ip->lock) {
    //     __ipaf_remove_socket(ip, socket);
    // }

    return 0;
}

static void ip_setup(struct address_family *fam)
{

}

static struct address_family_ops ip_address_family_ops = {
    .setup_af = ip_setup,
    .create = ip_create,
    .delete = ip_delete,
    .packet_rx = ip_rx,
};

struct procfs_dir *ipv4_dir_procfs;

void ip_init(void)
{
    address_family_register(&ip_address_family.af);

    ipv4_dir_procfs = procfs_register_dir(net_dir_procfs, "ipv4");

    procfs_register_entry_ops(ipv4_dir_procfs, "route", &ipv4_route_ops);
}
