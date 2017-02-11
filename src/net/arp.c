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

#include <protura/net.h>

struct arp_header {
    uint16_t hrd_type;
    uint16_t protocol_type;
    uint8_t hrd_addr_len;
    uint8_t protocol_addr_len;
    uint16_t opcode;

    uint8_t send_hrd_addr[6];
    in_addr_t send_inet_addr;

    uint8_t recv_hrd_addr[6];
    in_addr_t recv_inet_addr;
} __packed;

#define ARP_CACHE_SIZE 32

/* Mapping from ipv4 to MAC */
struct arp_entry {
    list_node_t cache_entry;

    time_t timeout;

    in_addr_t addr;
    uint8_t mac[6];

    struct net_interface *iface;

    unsigned int waiting_for_response :1;
    struct wait_queue packet_wait_queue;
};

static mutex_t arp_cache_lock = MUTEX_INIT(arp_cache_lock, "arp-cache-lock");
static int arp_cache_length;
static list_head_t arp_cache = LIST_HEAD_INIT(arp_cache);

static void arp_entry_init(struct arp_entry *entry)
{
    memset(entry, 0, sizeof(*entry));
    list_node_init(&entry->cache_entry);
    wait_queue_init(&entry->packet_wait_queue);
}

static void arp_send_response(in_addr_t dest, uint8_t *dest_mac, struct net_interface *iface)
{
    struct packet *arp_packet;
    struct arp_header header;

    /* Create a new broadcast packet */
    arp_packet = packet_new();
    arp_packet->iface_tx = netdev_dup(iface);
    arp_packet->ll_type = htons(ETH_P_ARP);
    memcpy(arp_packet->mac_dest, dest_mac, 6);

    memset(&header, 0, sizeof(header));
    header.hrd_type = htons(ARPHRD_ETHER);
    header.protocol_type = htons(ETH_P_IP);
    header.opcode = htons(ARPOP_REPLY);

    header.hrd_addr_len = 6;
    header.protocol_addr_len = 4;

    using_netdev_read(iface) {
        memcpy(header.send_hrd_addr, iface->mac, 6);
        header.send_inet_addr = iface->in_addr;
    }

    memcpy(header.recv_hrd_addr, dest_mac, 6);
    header.recv_inet_addr = dest;

    packet_add_header(arp_packet, &header, sizeof(header));

    packet_linklayer_tx(arp_packet);
}

static void arp_send_request(in_addr_t inet_addr, struct net_interface *iface)
{
    struct packet *arp_packet;
    struct arp_header header;

    /* Create a new broadcast packet */
    arp_packet = packet_new();
    arp_packet->iface_tx = netdev_dup(iface);
    arp_packet->ll_type = htons(ETH_P_ARP);
    memset(arp_packet->mac_dest, 0xFF, sizeof(arp_packet->mac_dest));

    memset(&header, 0, sizeof(header));
    header.hrd_type = htons(ARPHRD_ETHER);
    header.protocol_type = htons(ETH_P_IP);
    header.opcode = htons(ARPOP_REQUEST);

    header.hrd_addr_len = 6;
    header.protocol_addr_len = 4;

    using_netdev_read(iface) {
        memcpy(header.send_hrd_addr, iface->mac, 6);
        header.send_inet_addr = iface->in_addr;
    }

    memset(header.recv_hrd_addr, 0xFF, 6);
    header.recv_inet_addr = inet_addr;

    packet_add_header(arp_packet, &header, sizeof(header));

    packet_linklayer_tx(arp_packet);
}

void arp_handle_packet(struct packet *packet)
{
    struct arp_header *arp_head;
    struct net_interface *iface;

    arp_head = packet->head;

    if (!(arp_head->hrd_type == htons(ARPHRD_ETHER)) || !(arp_head->protocol_type == htons(ETH_P_IP)))
        return ;

    switch (ntohs(arp_head->opcode)) {
    case ARPOP_REPLY:
        kp(KP_NORMAL, "ARP Reply: "PRmac" has "PRin_addr"\n",
                      Pmac(arp_head->send_hrd_addr), Pin_addr(arp_head->send_inet_addr));

        using_mutex(&arp_cache_lock) {
            struct arp_entry *entry;
            list_foreach_entry(&arp_cache, entry, cache_entry) {
                if (entry->addr == arp_head->send_inet_addr) {
                    memcpy(entry->mac, arp_head->send_hrd_addr, 6);
                    entry->waiting_for_response = 0;

                    wait_queue_wake_all(&entry->packet_wait_queue);
                    break;
                }
            }
        }
        break;

    case ARPOP_REQUEST:
        iface = netdev_get_inet(arp_head->recv_inet_addr);

        if (iface) {
            arp_send_response(arp_head->send_inet_addr, arp_head->send_hrd_addr, iface);
            netdev_put(iface);
        }

        break;
    }

    packet_free(packet);
}

static int __arp_wait_for_mac(struct arp_entry *entry, uint8_t *mac)
{
  sleep_again:
    sleep_with_wait_queue(&entry->packet_wait_queue) {
        if (!entry->waiting_for_response) {
            memcpy(mac, entry->mac, 6);
        } else {
            not_using_mutex(&arp_cache_lock)
                scheduler_task_yield();
            goto sleep_again;
        }
    }

    return 0;
}

int arp_ipv4_to_mac(in_addr_t inet_addr, uint8_t *mac, struct net_interface *iface)
{
    struct arp_entry *entry;
    int ret = 0, found = 0;

    using_mutex(&arp_cache_lock) {
        list_foreach_entry(&arp_cache, entry, cache_entry) {
            if (entry->addr == inet_addr) {
                kp(KP_NORMAL, "Found ARP entry.\n");
                ret = __arp_wait_for_mac(entry, mac);
                found = 1;
                break;
            }
        }

        if (!found) {
            /* No entry, create a new one */
            kp(KP_NORMAL, "Creating new arp_entry: %d\n", arp_cache_length);
            entry = kmalloc(sizeof(*entry), PAL_KERNEL);
            arp_entry_init(entry);

            entry->addr = inet_addr;
            entry->iface = iface;
            entry->waiting_for_response = 1;

            list_add_tail(&arp_cache, &entry->cache_entry);
            arp_cache_length++;

            kp(KP_NORMAL, "Sending ARP request for: "PRin_addr"\n", Pin_addr(inet_addr));
            arp_send_request(inet_addr, iface);
            kp(KP_NORMAL, "Waiting for ARP response.\n");

            ret = __arp_wait_for_mac(entry, mac);
        }
    }

    return ret;
}

