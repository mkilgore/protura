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
#include <arch/paging.h>
#include <arch/asm.h>
#include <arch/irq.h>
#include <arch/idt.h>
#include <arch/drivers/pic8259.h>

#include <protura/fs/procfs.h>
#include <protura/drivers/pci.h>
#include <protura/drivers/pci_ids.h>
#include <protura/net/arphrd.h>
#include <protura/net.h>
#include <protura/drivers/rtl.h>
#include <protura/net/arp.h>
#include <protura/net/linklayer.h>
#include "internal.h"

/* Defines the functions rtl_outb, rtl_outw, rtl_outl, rtl_inb, etc... */
DEFINE_REG_ACCESS_FUNCTIONS(rtl, struct net_interface_rtl *rtl, rtl->io_base);

static void rtl_handle_rx(struct net_interface_rtl *rtl)
{
    uint8_t cmd;

    while (!(cmd = rtl_inb(rtl, REG_CR) & REG_CR_BUFE)) {
        struct rtl_rx_packet *rx_buf;
        struct packet *packet;

        if (rtl->rx_cur_offset > 8192)
            rtl->rx_cur_offset %= 8192;

        rx_buf = rtl->rx_buffer->virt + rtl->rx_cur_offset;

        if (rx_buf->len > 0) {
            packet = packet_new();

            memcpy(packet->start, rx_buf->packet, rx_buf->len);
            packet->head = packet->start;
            packet->tail = packet->head + rx_buf->len;

            packet->iface_rx = netdev_dup(&rtl->net);

            net_packet_receive(packet);
        } else {
            kp(KP_NORMAL, "rtl8139: Recieved empty packet\n");
        }

        /* The header is 4 bytes long, and 3 is used to align the offset to 4-bytes */
        rtl->rx_cur_offset += (rx_buf->len + 4 + 3) & ~3;
        rx_buf = (struct rtl_rx_packet *)((char *)rx_buf + rx_buf->len + 2);

        rtl_outw(rtl, REG_CAPR, rtl->rx_cur_offset - 0x10); /* The subtraction avoids overflow issues */
    }
}

static void __rtl_send_packet(struct net_interface_rtl *rtl, struct packet *packet, size_t len)
{
    packet_to_buffer(packet, rtl->tx_buffer[rtl->tx_cur_buffer]->virt, PG_SIZE);

    rtl_outl(rtl, REG_TSD(rtl->tx_cur_buffer), len);
    rtl->tx_cur_buffer = (rtl->tx_cur_buffer + 1) % 4;
}

static void rtl_rx_interrupt(struct irq_frame *frame, void *param)
{
    struct net_interface_rtl *rtl = param;
    uint16_t isr = rtl_inw(rtl, REG_ISR);

    if (isr & REG_ISR_ROK)
        rtl_handle_rx(rtl);

    if (isr & REG_ISR_TOK) {
        /* 
         * Packet sent successfully
         * 
         * Check packet queue for any new packets, and send the next one in
         * line if so.
         */
        using_spinlock(&rtl->tx_lock) {
            /*
             * We do a loop here to attempt to queue up as many packets as we
             * can. it's possible that more then one TX buffer has become
             * availiable since the TOK was sent
             */
            while (!list_empty(&rtl->tx_packet_queue)
                   && (rtl_inl(rtl, REG_TSD(rtl->tx_cur_buffer)) & REG_TSD_OWN)) {
                struct packet *packet = list_take_first(&rtl->tx_packet_queue, struct packet, packet_entry);
                __rtl_send_packet(rtl, packet, packet_len(packet));
                packet_free(packet);
            }
        }
    }

    /* ACK Interrupt */
    rtl_outw(rtl, REG_ISR, isr);
}

static int rtl_packet_send(struct net_interface *iface, struct packet *packet)
{
    size_t len;
    struct net_interface_rtl *rtl = container_of(iface, struct net_interface_rtl, net);

    len = packet_len(packet);

    using_spinlock(&rtl->tx_lock) {
        int i, own = 0;

        /* We loop up to 10 times checking for the current buffer to become
         * usable.
         *
         * If it doesn't become usable then we add this packet to the queue, to be sent-off at a TOK. */
        for (i = 0; i < 10 && !own; i++)
            own = rtl_inl(rtl, REG_TSD(rtl->tx_cur_buffer)) & REG_TSD_OWN;

        if (own) {
            __rtl_send_packet(rtl, packet, len);
            packet_free(packet);
        } else {
            list_add_tail(&rtl->tx_packet_queue, &packet->packet_entry);
        }
    }

    return 0;
}

void rtl_device_init_rx(struct net_interface_rtl *rtl)
{
    rtl->rx_buffer = palloc(3, PAL_KERNEL); /* 4-page buffer, 16K */

    rtl_outl(rtl, REG_RCR, REG_RCR_AB | REG_RCR_AM | REG_RCR_APM | REG_RCR_AAP | REG_RCR_WRAP);
    rtl_outl(rtl, REG_RBSTART, page_to_pa(rtl->rx_buffer));
}

void rtl_device_init_tx(struct net_interface_rtl *rtl)
{
    int i;
    for (i = 0; i < 4; i++)
        rtl->tx_buffer[i] = palloc(0, PAL_KERNEL);

    rtl_outl(rtl, REG_TSAD0, (uintptr_t)page_to_pa(rtl->tx_buffer[0]));
    rtl_outl(rtl, REG_TSAD1, (uintptr_t)page_to_pa(rtl->tx_buffer[1]));
    rtl_outl(rtl, REG_TSAD2, (uintptr_t)page_to_pa(rtl->tx_buffer[2]));
    rtl_outl(rtl, REG_TSAD3, (uintptr_t)page_to_pa(rtl->tx_buffer[3]));

    rtl_outl(rtl, REG_TCR, REG_TCR_CRC);
}

void rtl_device_init(struct pci_dev *dev)
{
    struct net_interface_rtl *rtl = kzalloc(sizeof(*rtl), PAL_KERNEL);
    uint16_t command_reg;
    int int_line;

    net_interface_init(&rtl->net);

    spinlock_init(&rtl->tx_lock, "rtl8139-lock");
    list_head_init(&rtl->tx_packet_queue);
    rtl->net.packet_send = rtl_packet_send;
    rtl->net.linklayer_tx = packet_linklayer_tx;
    rtl->net.address_resolve = arp_tx;

    kp(KP_NORMAL, "Found RealTek RTL8139 Fast Ethernet: "PRpci_dev"\n", Ppci_dev(dev));

    rtl->net.name = "eth";
    rtl->dev = *dev;

    command_reg = pci_config_read_uint16(dev, PCI_COMMAND);
    pci_config_write_uint16(dev, PCI_COMMAND, command_reg | PCI_COMMAND_BUS_MASTER | PCI_COMMAND_IO_SPACE);

    int_line = pci_config_read_uint8(dev, PCI_REG_INTERRUPT_LINE);
    kp(KP_NORMAL, "  Interrupt: %d\n", int_line);

    irq_register_callback(PIC8259_IRQ0 + int_line, rtl_rx_interrupt, "RealTek RTL8239", IRQ_INTERRUPT, rtl);
    pic8259_enable_irq(int_line);

    rtl->io_base = pci_config_read_uint32(dev, PCI_REG_BAR(0)) & 0xFFFE;

    /* Turn the RTl8139 on */
    rtl_outb(rtl, REG_CONFIG0, 0);

    /* Perform a software reset.
     * REG_CR_RST will go low when reset is complete. */
    rtl_outb(rtl, REG_CR, REG_CR_RST);
    while (rtl_inb(rtl, REG_CR) & REG_CR_RST)
        ;

    /* Read MAC address */
    rtl->net.mac[0] = rtl_inb(rtl, REG_MAC0);
    rtl->net.mac[1] = rtl_inb(rtl, REG_MAC1);
    rtl->net.mac[2] = rtl_inb(rtl, REG_MAC2);
    rtl->net.mac[3] = rtl_inb(rtl, REG_MAC3);
    rtl->net.mac[4] = rtl_inb(rtl, REG_MAC4);
    rtl->net.mac[5] = rtl_inb(rtl, REG_MAC5);
    rtl->net.hwtype = ARPHRD_ETHER;

    rtl_device_init_rx(rtl);
    rtl_device_init_tx(rtl);

    rtl_outw(rtl, REG_IMR, REG_IMR_TOK | REG_IMR_ROK);
    rtl_outb(rtl, REG_CR, REG_CR_TE | REG_CR_RE);

    net_interface_register(&rtl->net);

    return ;
}

