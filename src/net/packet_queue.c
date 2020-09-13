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
#include <protura/mm/kmalloc.h>
#include <protura/snprintf.h>
#include <protura/list.h>
#include <protura/work.h>
#include <arch/asm.h>

#include <protura/fs/procfs.h>
#include <protura/drivers/pci.h>
#include <protura/drivers/pci_ids.h>
#include <protura/net/proto.h>
#include <protura/net/linklayer.h>
#include <protura/net.h>

static atomic_t queue_count = ATOMIC_INIT(0);
static struct workqueue packet_queue = WORKQUEUE_INIT(packet_queue);

static void packet_process(struct work *work)
{
    struct packet *packet = container_of(work, struct packet, dwork.work);

    int count = atomic_dec_return(&queue_count);

    if (unlikely(count > 30))
        kp(KP_WARNING, "Packet queue depth: %d\n", count);

    packet_linklayer_rx(packet);
}

void net_packet_receive(struct packet *packet)
{
    work_init_workqueue(&packet->dwork.work, packet_process, &packet_queue);
    flag_set(&packet->dwork.work.flags, WORK_ONESHOT);

    atomic_inc(&queue_count);
    work_schedule(&packet->dwork.work);
}

struct packet *__net_iface_tx_packet_pop(struct net_interface *iface)
{
    if (list_empty(&iface->tx_packet_queue))
        return NULL;

    return list_take_first(&iface->tx_packet_queue, struct packet, packet_entry);
}

void net_packet_transmit(struct packet *packet)
{
    struct net_interface *iface = packet->iface_tx;

    if (flag_test(&iface->flags, NET_IFACE_UP)) {
        using_netdev_write(iface) {
            iface->metrics.tx_packets++;
            iface->metrics.tx_bytes += packet_len(packet);
        }

        using_spinlock(&iface->tx_lock)
            list_add_tail(&iface->tx_packet_queue, &packet->packet_entry);

        (iface->process_tx_queue) (iface);
    } else {
        packet_free(packet);
    }
}

static void net_packet_queue_init(void)
{
    workqueue_start(&packet_queue, "packet-queue");
}
initcall_subsys(net_packet_queue, net_packet_queue_init);
