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
#include <protura/net/ipv4/icmp.h>
#include <protura/net.h>
#include "ipv4.h"

#define ICMP_TYPE_ECHO_REPLY 0
#define ICMP_TYPE_ECHO_REQUEST 8

struct icmp_header {
    uint8_t type;
    uint8_t code;
    n16 chksum;
    n32 rest;
} __packed;

struct icmp_protocol {
    struct protocol proto;

    struct socket *icmp_socket;
};

static struct protocol_ops icmp_protocol_ops;

static struct icmp_protocol icmp_protocol = {
    .proto = PROTOCOL_INIT(icmp_protocol.proto, PROTOCOL_ICMP, &icmp_protocol_ops),
};

/*
 * This is spawn in a worker thread becaues we can't hang-up the
 * packet-processing thread while using the icmp socket, since the socket may
 * trigger other packets to need to be handled (For example, ARP packets).
 */

static void icmp_packet_work(struct work *work)
{
    struct packet *packet = container_of(work, struct packet, dwork.work);
    struct icmp_header *header = packet->head;
    struct sockaddr_in *src_in;
    int ret;

    struct ip_header *ip_head = packet->af_head;
    size_t icmp_len = ntohs(ip_head->total_length) - ip_head->ihl * 4;

    switch (header->type) {
    case ICMP_TYPE_ECHO_REQUEST:
        src_in = (struct sockaddr_in *)&packet->src_addr;
        header->type = ICMP_TYPE_ECHO_REPLY;
        header->chksum = 0;
        header->chksum = ip_chksum((uint16_t *)header, icmp_len);

        kp_icmp("Checksum: 0x%04X, Len: %d\n", header->chksum, icmp_len);

        ret = socket_sendto(icmp_protocol.icmp_socket, packet->head, packet_len(packet), 0, &packet->src_addr, packet->src_len, 0);
        kp_icmp("Reply to "PRin_addr": %d\n", Pin_addr(src_in->sin_addr.s_addr), ret);
        break;

    default:
        break;
    }

    packet_free(packet);
}

static void icmp_rx(struct protocol *proto, struct packet *packet)
{
    kwork_schedule_callback(&packet->dwork.work, icmp_packet_work);
}

static struct protocol_ops icmp_protocol_ops = {
    .packet_rx = icmp_rx,
};

void icmp_init(void)
{
    protocol_register(&icmp_protocol.proto);
}

void icmp_init_delay(void)
{
    int ret;
    ret = socket_open(AF_INET, SOCK_RAW, IPPROTO_ICMP, &icmp_protocol.icmp_socket);
    if (ret)
        panic("Error opening ICMP Socket!\n");
}

