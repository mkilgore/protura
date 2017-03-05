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
#include <protura/kassert.h>
#include <protura/dump_mem.h>
#include <protura/mm/kmalloc.h>
#include <protura/snprintf.h>
#include <protura/list.h>
#include <arch/asm.h>

#include <protura/net/udp.h>
#include <protura/net/socket.h>
#include <protura/net/proto.h>
#include <protura/net/netdevice.h>
#include <protura/net/arphrd.h>
#include <protura/net/af/ipv4.h>
#include <protura/net/af/ip_route.h>
#include <protura/net.h>
#include "ipv4.h"

struct ip_header {
    uint8_t ihl :4;
    uint8_t version :4;

    uint8_t tos;

    uint16_t total_length;

    uint16_t id;

    uint16_t frag_off;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t csum;
    uint32_t source_ip;
    uint32_t dest_ip;
} __packed;

struct ip_private {
    in_addr_t bind_addr;
};

struct address_family_ip {
    struct address_family af;

    mutex_t lock;

    list_head_t raw_sockets[256];

    struct protocol *protos[256];
};

static struct address_family_ops ip_address_family_ops;

static struct address_family_ip ip_address_family = {
    .af = ADDRESS_FAMILY_INIT(ip_address_family.af),
    .af = {
        .af_type = AF_INET,
        .ops = &ip_address_family_ops,
    },
    .lock = MUTEX_INIT(ip_address_family.lock, "ip-address-family-lock"),
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

    return (vals[3] << 24) + (vals[2] << 16) + (vals[1] << 8) + vals[0];
}

static uint16_t ip_chksum(struct ip_header *header)
{
    uint16_t *head = (uint16_t *)header;
    int len = header->ihl * 2;
    int i;
    uint32_t sum = 0;

    for (i = 0; i < len; i++)
        sum += head[i];

    while (sum & 0xFFFF0000)
        sum = (sum & 0xFFFF) + ((sum & 0xFFFF0000) >> 16);

    return ~sum;
}

static uint16_t next_ip_id = 0;

void ip_rx(struct address_family *afamily, struct packet *packet)
{
    struct address_family_ip *af = container_of(afamily, struct address_family_ip, af);
    struct ip_header *header = packet->head;
    struct sockaddr_in *in;
    struct socket *socket;

    packet->head += header->ihl * 4;

    kp(KP_NORMAL, "IP packet: "PRin_addr" -> "PRin_addr"\n", Pin_addr(header->source_ip), Pin_addr(header->dest_ip));
    kp(KP_NORMAL, "  Version: %d, HL: %d, %d bytes\n", header->version, header->ihl, header->ihl * 4);
    kp(KP_NORMAL, "  Protocol: 0x%04x, ID: 0x%04x, Len: 0x%04x\n", ntohs(header->protocol), ntohs(header->id), ntohs(header->total_length) - header->ihl * 4);

    in = (struct sockaddr_in *)&packet->src_addr;

    in->sin_family = AF_INET;
    in->sin_addr.s_addr = header->source_ip;
    packet->src_len = sizeof(*in);

    using_mutex(&af->lock) {
        list_foreach_entry(&af->raw_sockets[header->protocol], socket, socket_entry) {
            struct packet *dup_packet = packet_dup(packet);

            using_mutex(&socket->recv_lock) {
                list_add_tail(&socket->recv_queue, &dup_packet->packet_entry);
                wait_queue_wake(&socket->recv_wait_queue);
            }
        }

        if (af->protos[header->protocol])
            (af->protos[header->protocol]->ops->packet_rx) (af->protos[header->protocol], packet);
        else
            packet_free(packet);
    }
}

static void ip_build_header(struct address_family *af, struct packet *packet, struct ip_route_entry *ip)
{
    struct ip_header *header;
    size_t data_len;

    data_len = packet_len(packet);

    packet->head -= sizeof(struct ip_header);
    header = packet->head;

    memset(header, 0, sizeof(*header));

    header->version = 4;
    header->ihl = 5;
    header->tos = 0;
    header->id = htons(next_ip_id++);
    header->frag_off = 0;
    header->ttl = 30;
    header->protocol = packet->protocol_type;
    header->total_length = htons(data_len + sizeof(*header));

    using_netdev_read(ip->iface)
        header->source_ip = ip->iface->in_addr;

    if (ip->flags & IP_ROUTE_GATEWAY)
        header->dest_ip = ip->gateway_ip;
    else
        header->dest_ip = ip->dest_ip;

    header->csum = 0;
    header->csum = ip_chksum(header);

    kp(KP_NORMAL, "Ip route: "PRin_addr", len: %d, csum: 0x%04x\n", Pin_addr(ip->gateway_ip), data_len, header->csum);
}

static int ip_sendto(struct address_family *af, struct socket *sock, struct packet *packet, const struct sockaddr *addr, socklen_t len)
{
    struct ip_route_entry route;


    if (addr) {
        const struct sockaddr_in *in;

        if (sizeof(*in) > len)
            return -EFAULT;

        in = (const struct sockaddr_in *)addr;

        int ret = ip_route_get(in->sin_addr.s_addr, &route);
        if (ret)
            return -EACCES;
    } else if (flag_test(&sock->flags, SOCKET_IS_BOUND)) {
        int ret = ip_route_get(sock->af_private.ipv4.bind_addr, &route);
        if (ret)
            return -EACCES;
    } else {
        return -EDESTADDRREQ;
    }

    ip_build_header(af, packet, &route);
    packet->ll_type = htons(ETH_P_IP);
    memcpy(packet->dest_mac, route.dest_mac, 6);
    packet->iface_tx = netdev_dup(route.iface);

    ip_route_clear(&route);

    return 0;
}

static int ip_create(struct address_family *family, struct socket *socket)
{
    struct address_family_ip *ip = (struct address_family_ip *)family;

    if (socket->sock_type == SOCK_DGRAM
        && (socket->protocol == 0 || socket->protocol == IPPROTO_UDP)) {
        socket->proto = protocol_lookup(PROTOCOL_UDP);
    } else if (socket->sock_type == SOCK_RAW) {
        if (socket->protocol > 255 || socket->protocol < 0)
            return -EINVAL;

        using_mutex(&ip->lock)
            list_add_tail(&ip->raw_sockets[socket->protocol], &socket->socket_entry);

    } else {
        return -EINVAL;
    }

    return 0;
}

static int ip_delete(struct address_family *family, struct socket *socket)
{
    return 0;
}

static void ip_setup(struct address_family *fam)
{
    struct address_family_ip *ip = (struct address_family_ip *)fam;

    using_mutex(&ip->lock) {
        ip->protos[IPPROTO_UDP] = protocol_lookup(PROTOCOL_UDP);
    }
}

static int ip_bind(struct address_family *family, struct socket *sock, const struct sockaddr *addr, socklen_t len)
{
    const struct sockaddr_in *in = (const struct sockaddr_in *)addr;

    if (sizeof(*in) > len)
        return -EFAULT;

    sock->af_private.ipv4.bind_addr = in->sin_addr.s_addr;

    return 0;
}

static int ip_autobind(struct address_family *family, struct socket *sock)
{
    sock->af_private.ipv4.bind_addr = 0;

    return 0;
}

static int ip_getsockname(struct address_family *family, struct socket *sock, struct sockaddr *addr, socklen_t *len)
{
    struct sockaddr_in *in = (struct sockaddr_in *)addr;

    if (*len < sizeof(*in))
        return -EFAULT;

    in->sin_addr.s_addr = sock->af_private.ipv4.bind_addr;
    *len = sizeof(*in);

    return 0;
}

static struct address_family_ops ip_address_family_ops = {
    .setup_af = ip_setup,
    .packet_rx = ip_rx,
    .create = ip_create,
    .delete = ip_delete,
    .sendto = ip_sendto,

    .bind = ip_bind,
    .autobind = ip_autobind,
    .getsockname = ip_getsockname,
};

struct procfs_dir *ipv4_dir_procfs;

void ip_init(void)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(ip_address_family.raw_sockets); i++)
        list_head_init(ip_address_family.raw_sockets + i);

    address_family_register(&ip_address_family.af);

    ipv4_dir_procfs = procfs_register_dir(net_dir_procfs, "ipv4");

    procfs_register_entry_ops(ipv4_dir_procfs, "route", &ipv4_route_ops);
}

