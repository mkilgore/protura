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

#include <protura/net.h>

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

static char mem[4096];

void ip_rx(struct packet *packet)
{
    struct ip_header *header = packet->head;

    packet->head += header->ihl * 4;

    kp(KP_NORMAL, "IP packet: "PRin_addr" -> "PRin_addr"\n", Pin_addr(header->source_ip), Pin_addr(header->dest_ip));
    kp(KP_NORMAL, "  Version: %d, HL: %d, %d bytes\n", header->version, header->ihl, header->ihl * 4);
    kp(KP_NORMAL, "  Protocol: 0x%04x, ID: 0x%04x, Len: 0x%04x\n", ntohs(header->protocol), ntohs(header->id), ntohs(header->total_length) - header->ihl * 4);

    dump_mem(mem, sizeof(mem), packet->head, ntohs(header->total_length) - header->ihl * 4, 0);

    kp(KP_NORMAL, "\n%s", mem);

    packet_free(packet);
}

void ip_tx(struct packet *packet)
{

}

