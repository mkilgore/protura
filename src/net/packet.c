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
    memset(packet->mac_dest, 0, 6);

    if (packet->iface_tx) {
        netdev_put(packet->iface_tx);
        packet->iface_tx = NULL;
    }
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

struct packet *packet_dup(struct packet *packet)
{
    struct packet *dup_packet = packet_new();

    dup_packet->head = dup_packet->start + (packet->head - packet->start);
    dup_packet->tail = dup_packet->start + (packet->tail - packet->start);

    memcpy(dup_packet->head, packet->head, packet_len(packet));

    return dup_packet;
}

void packet_add_header(struct packet *packet, void *header, size_t header_len)
{
    packet->head -= header_len;
    memcpy(packet->head, header, header_len);
}

void packet_append_data(struct packet *packet, void *data, size_t data_len)
{
    memcpy(packet->tail, data, data_len);
    packet->tail += data_len;
}

void packet_pad_zero(struct packet *packet, size_t len)
{
    memset(packet->tail, 0, len);
    packet->tail += len;
}

