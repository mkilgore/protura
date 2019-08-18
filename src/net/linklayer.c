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
#include <protura/spinlock.h>
#include <protura/mm/kmalloc.h>
#include <protura/snprintf.h>
#include <protura/list.h>

#include <protura/net/ipv4/ipv4.h>
#include <protura/net.h>
#include <protura/net/arphrd.h>
#include <protura/net/arp.h>
#include <protura/net/linklayer.h>

struct ether_header {
    char mac_dest[6];
    char mac_src[6];

    uint16_t ether_type;
} __packed;

struct ether {
    struct address_family *ip, *arp;
};

static struct ether ether;

void packet_linklayer_rx(struct packet *packet)
{
    struct ether_header *ehead;

    using_netdev_write(packet->iface_rx) {
        packet->iface_rx->metrics.rx_packets++;
        packet->iface_rx->metrics.rx_bytes += packet_len(packet);
    }

    ehead = packet->head;
    packet->ll_head = ehead;

    packet->head += sizeof(struct ether_header);

    switch (ntohs(ehead->ether_type)) {
    case ETH_P_ARP:
        kp(KP_NORMAL, "ARP packet\n");
        (ether.arp->ops->packet_rx) (ether.arp, packet);
        break;

    case ETH_P_IP:
        kp(KP_NORMAL, "IP packet\n");
        (ether.ip->ops->packet_rx) (ether.ip, packet);
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
int packet_linklayer_tx(struct packet *packet)
{
    struct ether_header ehead;
    memcpy(ehead.mac_dest, packet->dest_mac, sizeof(ehead.mac_dest));
    memcpy(ehead.mac_src, packet->iface_tx->mac, sizeof(ehead.mac_src));
    ehead.ether_type = packet->ll_type;

    kp(KP_NORMAL, "Ether type: 0x%04x\n", ehead.ether_type);

    if (packet_len(packet) + 14 < 60)
        packet_pad_zero(packet, 60 - (packet_len(packet) + 14));

    packet_add_header(packet, &ehead, sizeof(ehead));

    if (flag_test(&packet->iface_tx->flags, NET_IFACE_UP)) {
        using_netdev_write(packet->iface_tx) {
            packet->iface_tx->metrics.tx_packets++;
            packet->iface_tx->metrics.tx_bytes += packet_len(packet);
        }

        (packet->iface_tx->packet_send) (packet->iface_tx, packet);
    } else {
        packet_free(packet);
    }

    return 0;
}

void linklayer_setup(void)
{
    ether.ip = address_family_lookup(AF_INET);
    ether.arp = address_family_lookup(AF_ARP);
}

