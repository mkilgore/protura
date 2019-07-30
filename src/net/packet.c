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

#include <protura/fs/procfs.h>
#include <protura/drivers/pci.h>
#include <protura/drivers/pci_ids.h>
#include <protura/net.h>

static spinlock_t packet_list_lock = SPINLOCK_INIT("packet-free-list-lock");
static list_head_t packet_free_list = LIST_HEAD_INIT(packet_free_list);

static void packet_clear(struct packet *packet)
{
    packet->head = packet->start + PACKET_RESERVE_HEADER_SPACE;
    packet->tail = packet->head;

    packet->ll_type = 0;
    memset(&packet->dest_mac, 0, sizeof(packet->dest_mac));

    packet->route_addr = 0;
    packet->protocol_type = 0;

    memset(&packet->dest_addr, 0, sizeof(packet->dest_addr));
    packet->dest_len = 0;

    memset(&packet->src_addr, 0, sizeof(packet->src_addr));
    packet->src_len = 0;

    if (packet->iface_tx) {
        netdev_put(packet->iface_tx);
        packet->iface_tx = NULL;
    }

    if (packet->iface_rx) {
        netdev_put(packet->iface_rx);
        packet->iface_rx = NULL;
    }

    if (packet->sock) {
        socket_put(packet->sock);
        packet->sock = NULL;
    }

    packet->ll_head = NULL;
    packet->af_head = NULL;
    packet->proto_head = NULL;
}

struct packet *packet_new(void)
{
    struct packet *packet = NULL;

    using_spinlock(&packet_list_lock)
        if (!list_empty(&packet_free_list))
            packet = list_take_first(&packet_free_list, struct packet, packet_entry);

    if (!packet) {
        packet = kmalloc(sizeof(*packet), PAL_KERNEL);
        packet_init(packet);
        packet->start = palloc_va(0, PAL_KERNEL);
        packet->head = packet->start + PACKET_RESERVE_HEADER_SPACE;
        packet->tail = packet->head;
        packet->end = packet->head + PG_SIZE;
    }

    return packet;
}

void packet_free(struct packet *packet)
{
    packet_clear(packet);

    using_spinlock(&packet_list_lock)
        list_add_tail(&packet_free_list, &packet->packet_entry);
}

struct packet *packet_copy(struct packet *packet)
{
    struct packet *dup_packet = packet_new();

    dup_packet->head = dup_packet->start + (packet->head - packet->start);
    dup_packet->tail = dup_packet->start + (packet->tail - packet->start);
    memcpy(dup_packet->head, packet->head, packet_len(packet));

    dup_packet->flags = packet->flags;

    dup_packet->ll_type = packet->ll_type;
    memcpy(dup_packet->dest_mac, packet->dest_mac, sizeof(packet->dest_mac));

    dup_packet->route_addr = packet->route_addr;
    dup_packet->protocol_type = packet->protocol_type;

    dup_packet->dest_addr = packet->dest_addr;
    dup_packet->dest_len  = packet->dest_len;

    dup_packet->src_addr = packet->src_addr;
    dup_packet->src_len = packet->src_len;

    if (packet->iface_tx)
        dup_packet->iface_tx = netdev_dup(packet->iface_tx);

    if (packet->iface_rx)
        dup_packet->iface_rx = netdev_dup(packet->iface_rx);

    if (packet->sock)
        dup_packet->sock = socket_dup(packet->sock);

    return dup_packet;
}

void packet_add_header(struct packet *packet, const void *header, size_t header_len)
{
    packet->head -= header_len;
    memcpy(packet->head, header, header_len);
}

void packet_append_data(struct packet *packet, const void *data, size_t data_len)
{
    memcpy(packet->tail, data, data_len);
    packet->tail += data_len;
}

void packet_pad_zero(struct packet *packet, size_t len)
{
    memset(packet->tail, 0, len);
    packet->tail += len;
}

