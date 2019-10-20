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
#include <protura/dump_mem.h>
#include <protura/mm/kmalloc.h>
#include <protura/snprintf.h>
#include <protura/list.h>
#include <arch/asm.h>

#include <protura/net/socket.h>
#include <protura/net/ipv4/icmp.h>
#include <protura/net.h>
#include "ipv4.h"

#define ICMP_TYPE_ECHO_REPLY 0
#define ICMP_TYPE_ECHO_REQUEST 8

struct icmp_header {
    uint8_t type;
    uint8_t code;
    n16 chksum;
    n32 rest;
} __packed;

static struct task *icmp_sock_thread;
static struct socket *icmp_socket;

static void icmp_handle_packet(struct packet *packet)
{
    struct icmp_header *header = packet->head;
    struct sockaddr_in *src_in;

    struct ip_header *ip_head = packet->af_head;
    size_t icmp_len = ntohs(ip_head->total_length) - ip_head->ihl * 4;

    kp_icmp("af_head: %p, start: %p\n", packet->af_head, packet->start);
    kp_icmp("ip total length: %d, HL: %d\n", ntohs(ip_head->total_length), ip_head->ihl);
    kp_icmp("tail length: %ld\n", packet->tail - packet->head);
    kp_icmp("  Packet: "PRin_addr" -> "PRin_addr"\n", Pin_addr(ip_head->source_ip), Pin_addr(ip_head->dest_ip));
    kp_icmp("  Version: %d, HL: %d, %d bytes\n", ip_head->version, ip_head->ihl, ip_head->ihl * 4);
    kp_icmp("  Protocol: 0x%02x, ID: 0x%04x, Len: 0x%04x\n", ip_head->protocol, ntohs(ip_head->id), ntohs(ip_head->total_length) - ip_head->ihl * 4);

    switch (header->type) {
    case ICMP_TYPE_ECHO_REQUEST:
        src_in = (struct sockaddr_in *)&packet->src_addr;
        header->type = ICMP_TYPE_ECHO_REPLY;
        header->chksum = htons(0);
        header->chksum = ip_chksum((uint16_t *)header, icmp_len);

        kp_icmp("Checksum: 0x%04X, Len: %d\n", ntohs(header->chksum), icmp_len);

        socket_sendto(icmp_socket, packet->head, packet_len(packet), 0, &packet->src_addr, packet->src_len, 0);
        kp_icmp("Reply to "PRin_addr": %d\n", Pin_addr(src_in->sin_addr.s_addr), ret);
        break;

    default:
        break;
    }

    packet_free(packet);
}

static int icmp_handler(void *ptr)
{
    struct socket *sock = ptr;
    while (1) {
        struct packet *packet;

        using_mutex(&sock->recv_lock) {
            wait_queue_event_mutex(&sock->recv_wait_queue, !list_empty(&sock->recv_queue), &sock->recv_lock);
            packet = list_take_first(&sock->recv_queue, struct packet, packet_entry);
        }

        icmp_handle_packet(packet);
    }

    return 0;
}

void icmp_init(void)
{
    int ret;
    ret = socket_open(AF_INET, SOCK_RAW, IPPROTO_ICMP, &icmp_socket);
    if (ret)
        panic("Error opening ICMP Socket!\n");

    icmp_sock_thread = task_kernel_new_interruptable("icmp-thread", icmp_handler, icmp_socket);
    scheduler_task_add(icmp_sock_thread);
}
