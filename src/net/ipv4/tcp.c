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
#include <protura/net/ipv4/tcp.h>
#include <protura/net/ipv4/ipv4.h>
#include <protura/net.h>
#include "ipv4.h"

#define TCP_LOWEST_AUTOBIND_PORT 50000

/* These are expected to be *host byte order* */
struct tcp_lookup {
    in_addr_t src_addr;
    in_port_t src_port;

    in_addr_t dest_addr;
    in_port_t dest_port;
};

struct tcp_protocol {
    struct protocol proto;

    mutex_t lock;
    uint16_t next_port;
};

static struct protocol_ops tcp_protocol_ops;

static struct tcp_protocol tcp_protocol = {
    .proto = PROTOCOL_INIT(tcp_protocol.proto, PROTOCOL_TCP, &tcp_protocol_ops),
    .lock = MUTEX_INIT(tcp_protocol.lock, "tcp-protocol-lock"),
    .next_port = TCP_LOWEST_AUTOBIND_PORT,
};

struct pseudo_header {
    n32 saddr;
    n32 daddr;
    uint8_t zero;
    uint8_t proto;
    n16 len;
} __packed;

static uint32_t sum_every_16(const void *data, size_t len)
{
    uint32_t sum = 0;
    const uint16_t *ptr = data;

    for (; len > 1; len -= 2, ptr++)
        sum += *ptr;

    if (len > 0)
        sum += *(uint8_t *)ptr;

    return sum;
}

static n16 tcp_checksum(struct pseudo_header *header, const char *data, size_t len)
{
    uint32_t sum = 0;

    sum = sum_every_16(header, sizeof(*header));
    sum += sum_every_16(data, len);

    /* Fold bits over to get the one's complement sum */
    while ((sum >> 16) != 0)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return htons(sum);
}

static int tcp_tx(struct protocol *proto, struct packet *packet)
{
    return 0;
}

void tcp_lookup_fill(struct ip_lookup *lookup, struct packet *packet)
{
    struct tcp_header *tcp_head = packet->head;

    lookup->src_port = tcp_head->dest;
    lookup->dest_port = tcp_head->source;
}

static void tcp_transmit(struct work *work)
{
    struct packet *packet = container_of(work, struct packet, dwork.work);
    struct sockaddr_in sockaddr;

    memcpy(&sockaddr, &packet->src_addr, sizeof(sockaddr));

    packet_free(packet);
}

static void tcp_rx(struct protocol *proto, struct packet *packet)
{
    struct tcp_header *tcp_head = packet->head;
    struct pseudo_header pseudo_header;
    struct ip_header *ip_head = packet->af_head;

    memset(&pseudo_header, 0, sizeof(pseudo_header));

    pseudo_header.saddr = ip_head->source_ip;
    pseudo_header.daddr = ip_head->dest_ip;
    pseudo_header.proto = ip_head->protocol;
    pseudo_header.len = htons(packet_len(packet));

    n16 checksum = tcp_checksum(&pseudo_header, packet->head, packet_len(packet));

    kp_tcp("packet: %d -> %d, %d bytes\n", ntohs(tcp_head->source), ntohs(tcp_head->dest), packet_len(packet));
    kp_tcp("CHKSUM: %s\n", (ntohs(checksum) == 0xFFFF)? "Correct": "Incorrect");

    n16 tmp = tcp_head->dest;
    tcp_head->dest = tcp_head->source;
    tcp_head->source = tmp;

    if (tcp_head->syn) {
        tcp_head->ack = 1;
        tcp_head->ack_seq = htonl(ntohl(tcp_head->seq) + 1);
        tcp_head->seq = htonl(1234);
    }

    packet->tail -= (tcp_head->hl - 5) * 4;

    tcp_head->hl = 5;
    tcp_head->check = htons(0);

    tcp_head->check = tcp_checksum(&pseudo_header, packet->head, packet_len(packet));

    packet->protocol_type = IPPROTO_TCP;

    work_init_kwork(&packet->dwork.work, tcp_transmit);
    flag_set(&packet->dwork.work.flags, WORK_ONESHOT);
    work_schedule(&packet->dwork.work);
}

static struct protocol_ops tcp_protocol_ops = {
    .packet_rx = tcp_rx,
};

void tcp_init(void)
{
    protocol_register(&tcp_protocol.proto);
}

