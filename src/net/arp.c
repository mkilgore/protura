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

#include <protura/ktimer.h>
#include <protura/net.h>
#include <protura/net/linklayer.h>
#include <protura/net/arphrd.h>
#include <protura/net/arp.h>

#define ARP_MAX_RESPONSE_TIME 500

struct arp_header {
    n16 hrd_type;
    n16 protocol;
    uint8_t hrd_addr_len;
    uint8_t protocol_addr_len;
    n16 opcode;

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

    list_head_t packet_queue;
};

/*
 * Order of locking is:
 *
 * arp_cache_lock:
 *   arp_entry
 */
static mutex_t arp_cache_lock = MUTEX_INIT(arp_cache_lock);
static int arp_cache_length;
static list_head_t arp_cache = LIST_HEAD_INIT(arp_cache);

static void arp_entry_init(struct arp_entry *entry)
{
    memset(entry, 0, sizeof(*entry));
    list_node_init(&entry->cache_entry);
    ktimer_init(&entry->timeout_timer);
    spinlock_init(&entry->lock);
    atomic_inc(&entry->refs);
    list_head_init(&entry->packet_queue);
}

static void arp_send_response(in_addr_t dest, uint8_t *dest_mac, struct net_interface *iface)
{
    struct packet *arp_packet;
    struct arp_header header;

    /* Create a new broadcast packet */
    arp_packet = packet_new(PAL_KERNEL);
    arp_packet->iface_tx = netdev_dup(iface);
    arp_packet->ll_type = htons(ETH_P_ARP);
    memcpy(arp_packet->dest_mac, dest_mac, 6);

    memset(&header, 0, sizeof(header));
    header.hrd_type = htons(ARPHRD_ETHER);
    header.protocol = htons(ETH_P_IP);
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
    arp_packet = packet_new(PAL_KERNEL);
    arp_packet->iface_tx = netdev_dup(iface);
    arp_packet->ll_type = htons(ETH_P_ARP);
    memset(arp_packet->dest_mac, 0xFF, sizeof(arp_packet->dest_mac));

    memset(&header, 0, sizeof(header));
    header.hrd_type = htons(ARPHRD_ETHER);
    header.protocol = htons(ETH_P_IP);
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

    kp(KP_NORMAL, "ARP Timeout\n");

    using_spinlock(&entry->lock) {
        kp(KP_NORMAL, "ARP waiting_for_response: %d\n", entry->waiting_for_response);
        if (entry->waiting_for_response) {
            entry->waiting_for_response = 0;
            entry->does_not_exist = 1;
            struct packet *packet;

            kp(KP_NORMAL, "Draining ARP packet queue...\n");
            list_foreach_take_entry(&entry->packet_queue, packet, packet_entry) {
                kp(KP_NORMAL, "ARP: Packet sock: %p\n", packet->sock);
                if (packet->sock)
                    socket_set_last_error(packet->sock, -EHOSTUNREACH);

                packet_free(packet);
            }
        }
    }
}

static void arp_process_packet_queue(struct arp_entry *entry)
{
    struct packet *packet;

    list_foreach_entry(&entry->packet_queue, packet, packet_entry)
        memcpy(packet->dest_mac, entry->mac, 6);
}

static void arp_process_packet_list(list_head_t *packet_list)
{
    struct packet *packet;

    list_foreach_take_entry(packet_list, packet, packet_entry)
        packet_linklayer_tx(packet);
}

static void arp_handle_packet(struct address_family *af, struct packet *packet)
{
    struct arp_header *arp_head;
    struct net_interface *iface;
    list_head_t packets_to_process = LIST_HEAD_INIT(packets_to_process);

    arp_head = packet->head;

    if (!n16_equal(arp_head->hrd_type, htons(ARPHRD_ETHER)) || !n16_equal(arp_head->protocol, htons(ETH_P_IP)))
        return ;

    switch (ntohs(arp_head->opcode)) {
    case ARPOP_REPLY:
        kp(KP_NORMAL, "ARP Reply: "PRmac" has "PRin_addr"\n",
                      Pmac(arp_head->send_hrd_addr), Pin_addr(arp_head->send_inet_addr));

        using_mutex(&arp_cache_lock) {
            struct arp_entry *entry;
            list_foreach_entry(&arp_cache, entry, cache_entry) {
                using_spinlock(&entry->lock) {
                    if (n32_equal(entry->addr, arp_head->send_inet_addr)) {
                        timer_del(&entry->timeout_timer);

                        memcpy(entry->mac, arp_head->send_hrd_addr, 6);
                        entry->waiting_for_response = 0;
                        entry->does_not_exist = 0;

                        arp_process_packet_queue(entry);

                        /* Move the list to our local list_head_t for processing outside the locks */
                        list_replace_init(&entry->packet_queue, &packets_to_process);
                        break;
                    }
                }
            }
        }
        break;

    case ARPOP_REQUEST:
        iface = netdev_get_inet(arp_head->recv_inet_addr);

        kp(KP_NORMAL, "ARP "PRmac" and "PRin_addr" to "PRmac" and "PRin_addr"\n",
                      Pmac(arp_head->send_hrd_addr), Pin_addr(arp_head->send_inet_addr),
                      Pmac(arp_head->recv_hrd_addr), Pin_addr(arp_head->recv_inet_addr));
        if (iface) {
            kp(KP_NORMAL, "ARP: Sending response on iface %s\n", iface->netdev_name);
            arp_send_response(arp_head->send_inet_addr, arp_head->send_hrd_addr, iface);
            netdev_put(iface);
        } else {
            kp(KP_NORMAL, "ARP: no iface to send response, we don't have that IP\n");
        }

        break;
    }

    arp_process_packet_list(&packets_to_process);

    packet_free(packet);
}

void arp_tx(struct packet *packet)
{
    struct arp_entry *entry;
    int found = 0;
    in_addr_t addr = packet->route_addr;

    using_mutex(&arp_cache_lock) {
        list_foreach_entry(&arp_cache, entry, cache_entry) {
            if (n32_equal(entry->addr, addr)) {
                found = 1;
                break;

            }
        }

        if (found) {
            using_spinlock(&entry->lock) {
                if (!entry->does_not_exist) {
                    memcpy(packet->dest_mac, entry->mac, 6);
                    goto pass_on_packet;
                } else {
                    /* Reset this entry to be sent tried again */
                    entry->waiting_for_response = 1;
                }

                list_add_tail(&entry->packet_queue, &packet->packet_entry);
            }
        } else {
            /* No entry, create a new one */
            kp(KP_NORMAL, "Creating new arp_entry: %d\n", arp_cache_length);
            entry = kmalloc(sizeof(*entry), PAL_KERNEL);
            arp_entry_init(entry);

            entry->timeout_timer.callback = arp_timeout_handler;

            entry->addr = addr;
            entry->iface = packet->iface_tx;
            entry->waiting_for_response = 1;

            list_add_tail(&entry->packet_queue, &packet->packet_entry);

            list_add_tail(&arp_cache, &entry->cache_entry);
            arp_cache_length++;
        }

        kp(KP_NORMAL, "Sending ARP request for: "PRin_addr"\n", Pin_addr(addr));
        arp_send_request(addr, packet->iface_tx);
        kp(KP_NORMAL, "Waiting for ARP response.\n");

        timer_add(&entry->timeout_timer, ARP_MAX_RESPONSE_TIME);
    }

    return;

  pass_on_packet:
    packet_linklayer_tx(packet);
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
    .af = ADDRESS_FAMILY_INIT(address_family_arp.af, AF_ARP, &arp_ops),
};

void arp_init(void)
{
    address_family_register(&address_family_arp.af);
}

