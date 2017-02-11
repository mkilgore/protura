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
#include <protura/spinlock.h>
#include <protura/mm/kmalloc.h>
#include <protura/snprintf.h>
#include <protura/list.h>

#include <protura/net/ip.h>
#include <protura/net.h>

struct ether_header {
    char mac_dest[6];
    char mac_src[6];

    uint16_t ether_type;
} __packed;

void packet_linklayer_rx(struct packet *packet)
{
    struct ether_header *ehead;

    ehead = packet->head;

    packet->head += sizeof(struct ether_header);

    switch (ntohs(ehead->ether_type)) {
    case ETH_P_ARP:
        arp_handle_packet(packet);
        break;

    case ETH_P_IP:
        ip_rx(packet);
        break;

    default:
        kp(KP_NORMAL, "Unknown ether packet type: 0x%04x\n", ehead->ether_type);
        packet_free(packet);
        break;
    }
}

/*
 * Only support ethernet right now...
 */
void packet_linklayer_tx(struct packet *packet)
{
    struct ether_header ehead;
    memcpy(ehead.mac_dest, packet->mac_dest, sizeof(ehead.mac_dest));
    memcpy(ehead.mac_src, packet->iface_tx->mac, sizeof(ehead.mac_src));
    ehead.ether_type = packet->ll_type;

    kp(KP_NORMAL, "Ether type: 0x%04x\n", ehead.ether_type);

    if (packet_len(packet) + 14 < 60)
        packet_pad_zero(packet, 60 - (packet_len(packet) + 14));

    packet_add_header(packet, &ehead, sizeof(ehead));

    (packet->iface_tx->packet_send) (packet->iface_tx, packet);
}

