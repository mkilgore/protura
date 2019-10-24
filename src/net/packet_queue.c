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

static struct workqueue packet_queue = WORKQUEUE_INIT(packet_queue);

static void packet_process(struct work *work)
{
    struct packet *packet = container_of(work, struct packet, dwork.work);

    kp(KP_NORMAL, "Recieved packet! len: %d\n", packet_len(packet));
    packet_linklayer_rx(packet);
}

void net_packet_receive(struct packet *packet)
{
    work_init_workqueue(&packet->dwork.work, packet_process, &packet_queue);
    flag_set(&packet->dwork.work.flags, WORK_ONESHOT);

    kp(KP_NORMAL, "Queued packet, length: %d\n", packet_len(packet));
    work_schedule(&packet->dwork.work);
}

void net_packet_transmit(struct packet *packet)
{
    packet_linklayer_tx(packet);
}

void net_packet_queue_init(void)
{
    workqueue_start(&packet_queue, "packet-queue");
}
