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
#include <arch/asm.h>

#include <protura/fs/procfs.h>
#include <protura/drivers/pci.h>
#include <protura/drivers/pci_ids.h>
#include <protura/net/proto.h>
#include <protura/net/linklayer.h>
#include <protura/net.h>

static struct task *packet_process_task;

static spinlock_t packet_queue_lock = SPINLOCK_INIT("packet-queue-lock");
static int packet_queue_length = 0;
static list_head_t packet_queue = LIST_HEAD_INIT(packet_queue);

static int packet_process_thread(void *p)
{
    while (1) {
        using_spinlock(&packet_queue_lock) {
            kp(KP_NORMAL, "packet-queue awake\n");

            while (!list_empty(&packet_queue)) {
                struct packet *packet = list_take_first(&packet_queue, struct packet, packet_entry);

                kp(KP_NORMAL, "Recieved packet! len: %d\n", packet_len(packet));
                kp(KP_NORMAL, "Packets in queue: %d\n", --packet_queue_length);

                not_using_spinlock(&packet_queue_lock)
                    packet_linklayer_rx(packet);
            }

            sleep {
                if (list_empty(&packet_queue)) {
                    not_using_spinlock(&packet_queue_lock)
                        scheduler_task_yield();
                }
            }
        }
    }

    return 0;
}

void net_packet_receive(struct packet *packet)
{
    using_spinlock(&packet_queue_lock) {
        list_add_tail(&packet_queue, &packet->packet_entry);
        packet_queue_length++;
    }

    kp(KP_NORMAL, "Queued packet, length: %d\n", packet_len(packet));

    if (packet_process_task) {
        kp(KP_NORMAL, "Waking packet processor\n");
        scheduler_task_wake(packet_process_task);
    }
}

void net_packet_transmit(struct packet *packet)
{
    packet_linklayer_tx(packet);
}

void net_packet_queue_init(void)
{
    struct task *thread = task_kernel_new_interruptable("packet-handler", packet_process_thread, NULL);
    scheduler_task_add(thread);

    atomic_ptr_swap(&packet_process_task, thread);
}

