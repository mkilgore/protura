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

#include <protura/ktimer.h>
#include <protura/net.h>
#include <protura/net/linklayer.h>
#include <protura/net/arphrd.h>
#include <protura/net/arp.h>

#define ARP_MAX_RESPONSE_TIME 500

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
    atomic_t refs;

    spinlock_t lock;

    struct ktimer timeout_timer;
    time_t timeout;

    in_addr_t addr;
    uint8_t mac[6];

    struct net_interface *iface;

    unsigned int waiting_for_response :1;
    unsigned int does_not_exist :1;
    struct wait_queue packet_wait_queue;
};

/*
 * Order of locking is:
 *
 * arp_cache_lock:
 *   arp_entry
 */
static mutex_t arp_cache_lock = MUTEX_INIT(arp_cache_lock, "arp-cache-lock");
static int arp_cache_length;
static list_head_t arp_cache = LIST_HEAD_INIT(arp_cache);

static void arp_entry_init(struct arp_entry *entry)
{
    memset(entry, 0, sizeof(*entry));
    list_node_init(&entry->cache_entry);
    ktimer_init(&entry->timeout_timer);
    spinlock_init(&entry->lock, "arp-entry-lock");
    wait_queue_init(&entry->packet_wait_queue);
    atomic_inc(&entry->refs);
}

static void arp_send_response(in_addr_t dest, uint8_t *dest_mac, struct net_interface *iface)
{
    struct packet *arp_packet;
    struct arp_header header;

    /* Create a new broadcast packet */
    arp_packet = packet_new();
    arp_packet->iface_tx = netdev_dup(iface);
    arp_packet->ll_type = htons(ETH_P_ARP);
    memcpy(arp_packet->dest_mac, dest_mac, 6);

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
    memset(arp_packet->dest_mac, 0xFF, sizeof(arp_packet->dest_mac));

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

static void arp_timeout_handler(struct ktimer *timer)
{
    struct arp_entry *entry = container_of(timer, struct arp_entry, timeout_timer);

    using_spinlock(&entry->lock) {
        if (entry->waiting_for_response) {
            entry->waiting_for_response = 0;
            entry->does_not_exist = 1;
            wait_queue_wake_all(&entry->packet_wait_queue);
        }
    }
}

static void arp_handle_packet(struct address_family *af, struct packet *packet)
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
                using_spinlock(&entry->lock) {
                    if (entry->addr == arp_head->send_inet_addr) {
                        timer_del(&entry->timeout_timer);

                        memcpy(entry->mac, arp_head->send_hrd_addr, 6);
                        entry->waiting_for_response = 0;
                        entry->does_not_exist = 0;

                        wait_queue_wake_all(&entry->packet_wait_queue);
                        break;
                    }
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
    int ret;

    wait_queue_event_spinlock(&entry->packet_wait_queue, !entry->waiting_for_response, &entry->lock);

    ret = entry->does_not_exist;
    if (!entry->does_not_exist)
        memcpy(mac, entry->mac, 6);

    return ret;
}

int arp_ipv4_to_mac(in_addr_t inet_addr, uint8_t *mac, struct net_interface *iface)
{
    struct arp_entry *entry;
    int ret = 0, found = 0;

    using_mutex(&arp_cache_lock) {
        list_foreach_entry(&arp_cache, entry, cache_entry) {
            if (entry->addr == inet_addr) {
                kp(KP_NORMAL, "Found ARP entry.\n");
                //ret = __arp_wait_for_mac(entry, mac);
                atomic_inc(&entry->refs);
                found = 1;
                break;
            }
        }

    }
    if (found) {
        ret = __arp_wait_for_mac(entry, mac);
        atomic_dec(&entry->refs);
    } else {
        /* No entry, create a new one */
        kp(KP_NORMAL, "Creating new arp_entry: %d\n", arp_cache_length);
        entry = kmalloc(sizeof(*entry), PAL_KERNEL);
        arp_entry_init(entry);

        entry->timeout_timer.callback = arp_timeout_handler;

        entry->addr = inet_addr;
        entry->iface = iface;
        entry->waiting_for_response = 1;

        using_mutex(&arp_cache_lock) {
            list_add_tail(&arp_cache, &entry->cache_entry);
            arp_cache_length++;
        }

        kp(KP_NORMAL, "Sending ARP request for: "PRin_addr"\n", Pin_addr(inet_addr));
        arp_send_request(inet_addr, iface);
        kp(KP_NORMAL, "Waiting for ARP response.\n");

        timer_add(&entry->timeout_timer, ARP_MAX_RESPONSE_TIME);

        using_spinlock(&entry->lock)
            ret = __arp_wait_for_mac(entry, mac);
    }

    return ret;
}

static void arp_setup_af(struct address_family *af)
{
    return ;
}

struct address_family_arp {
    struct address_family af;
};

static struct address_family_ops arp_ops = {
    .packet_rx = arp_handle_packet,
    .setup_af = arp_setup_af,
};

static struct address_family_arp address_family_arp = {
    .af = ADDRESS_FAMILY_INIT(address_family_arp.af),
    .af = {
        .af_type = AF_ARP,
        .ops = &arp_ops,
    },
};

void arp_init(void)
{
    address_family_register(&address_family_arp.af);
}

