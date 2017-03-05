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

#include <protura/net/socket.h>
#include <protura/net/proto/icmp.h>
#include <protura/net.h>

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
};

static struct protocol_ops icmp_protocol_ops;

static struct icmp_protocol icmp_protocol = {
    .proto = PROTOCOL_INIT(icmp_protocol.proto),
    .proto = {
        .protocol_id = PROTOCOL_ICMP,
        .ops = &icmp_protocol_ops,
    },
};

static void icmp_rx(struct protocol *proto, struct packet *packet)
{
    struct icmp_header *header;
}

static struct protocol_ops icmp_protocol_ops = {
    .packet_rx = icmp_rx,
};

void icmp_init(void)
{
    protocol_register(&icmp_protocol.proto);
}

