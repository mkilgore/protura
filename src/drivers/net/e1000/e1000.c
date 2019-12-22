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
#include <protura/mm/kmmap.h>
#include <protura/list.h>
#include <protura/snprintf.h>
#include <arch/asm.h>
#include <arch/irq.h>
#include <arch/idt.h>
#include <arch/drivers/pic8259.h>

#include <protura/fs/procfs.h>
#include <protura/drivers/pci.h>
#include <protura/drivers/pci_ids.h>
#include <protura/net/netdevice.h>
#include <protura/net/arp.h>
#include <protura/net/arphrd.h>
#include <protura/net.h>
#include <protura/drivers/e1000.h>
#include "e1000_internal.h"

/* 
 * When reading and writing to the E1000, you first write the address to the
 * 32-bit value at io_base, and then the register at that address is exposed in
 * io_base + 4 for read/write.
 */
static void e1000_command_write32(struct net_interface_e1000 *e1000, uint16_t addr, uint32_t value)
{
    if (e1000->mem_base) {
        *(uint32_t *)(e1000->mem_base + addr) = value;
    } else {
        outl(e1000->io_base, addr);
        outl(e1000->io_base + 4, value);
    }
}

static uint32_t e1000_command_read32(struct net_interface_e1000 *e1000, uint16_t addr)
{
    if (e1000->mem_base) {
        return *(uint32_t *)(e1000->mem_base + addr);
    } else {
        outl(e1000->io_base, addr);
        return inl(e1000->io_base + 4);
    }
}

static void e1000_eeprom_detect(struct net_interface_e1000 *e1000)
{
    int i;
    uint32_t result;

    e1000_command_write32(e1000, REG_EEPROM, 0x1);

    for (i = 0; i < 1000 && !e1000->has_eeprom; i++) {
        result = e1000_command_read32(e1000, REG_EEPROM);
        if (result & 0x10)
            e1000->has_eeprom = 1;
    }
}

/* NOTE: EEPROM addresses count 16-bit words, not 8-bit bytes */
static uint16_t e1000_eeprom_read(struct net_interface_e1000 *e1000, uint8_t addr)
{
    uint32_t result = 0;
    uint32_t eeprom_addr;

    eeprom_addr = (addr << 8) | 1;
    e1000_command_write32(e1000, REG_EEPROM, eeprom_addr);

    while (result = e1000_command_read32(e1000, REG_EEPROM),
           !(result & (1 << 4)))
        ;

    /* Read data is the top 16-bits */
    return (result >> 16);
}

static void e1000_read_mac(struct net_interface_e1000 *e1000)
{
    if (e1000->has_eeprom) {
        uint16_t tmp;
        tmp = e1000_eeprom_read(e1000, 0);
        e1000->net.mac[0] = tmp & 0xFF;
        e1000->net.mac[1] = tmp >> 8;

        tmp = e1000_eeprom_read(e1000, 1);
        e1000->net.mac[2] = tmp & 0xFF;
        e1000->net.mac[3] = tmp >> 8;

        tmp = e1000_eeprom_read(e1000, 2);
        e1000->net.mac[4] = tmp & 0xFF;
        e1000->net.mac[5] = tmp >> 8;

    } else {
        int i;
        for (i = 0; i < 6; i++) {
            e1000->net.mac[i] = ((uint8_t *)e1000->mem_base)[0x5400 + i];
        }
    }

    e1000->net.hwtype = ARPHRD_ETHER;
}

static void __e1000_rx_interrupt(struct net_interface_e1000 *e1000)
{
    int got_packet = 0;
    int old_rx = 0;

    while (e1000->rx_descs[e1000->cur_rx].status & RXD_STAT_DD) {
        struct e1000_rx_desc *desc = e1000->rx_descs + e1000->cur_rx;
        desc->status = 0;

        void *buf = P2V(desc->addr);

        if (desc->length < PG_SIZE) {
            struct packet *packet = packet_new(PAL_KERNEL | PAL_ATOMIC);

            memcpy(packet->start, buf, desc->length);
            packet->head = packet->start;
            packet->tail = packet->head + desc->length;

            packet->iface_rx = netdev_dup(&e1000->net);
            net_packet_receive(packet);
        } else {
            kp(KP_WARNING, "E1000: packet length too long: %d, dropped.\n", desc->length);
        }

        got_packet = 1;
        old_rx = e1000->cur_rx;
        e1000->cur_rx = (e1000->cur_rx + 1) % E1000_NUM_RX_DESC;
    }

    /* The RDT 'lags' the cur_rx by one */
    if (got_packet)
        e1000_command_write32(e1000, REG_RDT, old_rx);
}

static void __e1000_clear_tx_descs(struct net_interface_e1000 *e1000)
{
    struct e1000_tx_desc *descs = e1000->tx_descs;

    int next = (e1000->last_clear + 1) % E1000_NUM_TX_DESC;
    if (next == e1000->next_tx)
        return;

    /* Any packets with TSTA_DD set has already been sent by the hardware */
    while (descs[next].status & TSTA_DD) {
        descs[next].status = 0;
        pa_t page = PG_ALIGN_DOWN(descs[next].addr);

        pfree_pa(page, 0);

        e1000->last_clear = next;
        next = (next + 1) % E1000_NUM_TX_DESC;
    }
}

static int __e1000_tx_queue_has_space(struct net_interface_e1000 *e1000)
{
    return e1000->next_tx != e1000->last_clear;
}

static void __e1000_queue_tx(struct net_interface_e1000 *e1000, struct packet *packet)
{
    struct e1000_tx_desc *desc = e1000->tx_descs + e1000->next_tx;

    void *head = packet->head;
    size_t len = packet_len(packet);

    /* We steal the page from the packet so we don't need to do a copy. The
     * 'head' value we save above is inside of this page. */
    packet_take_page(packet);

    desc->cmd = TXD_CMD_RS | TXD_CMD_EOP | TXD_CMD_IFCS;
    desc->length = len;
    desc->addr = V2P(head);

    e1000->next_tx = (e1000->next_tx + 1) % E1000_NUM_TX_DESC;
    e1000_command_write32(e1000, REG_TDT, e1000->next_tx);

    packet_free(packet);
}

static void __e1000_flush_packet_queue(struct net_interface_e1000 *e1000)
{
    while (__e1000_tx_queue_has_space(e1000) && __net_iface_has_tx_packet(&e1000->net)) {
        struct packet *packet = __net_iface_tx_packet_pop(&e1000->net);

        __e1000_queue_tx(e1000, packet);
    }
}

static void e1000_process_tx_queue(struct net_interface *net)
{
    struct net_interface_e1000 *e1000 = container_of(net, struct net_interface_e1000, net);

    using_spinlock(&e1000->net.tx_lock) {
        __e1000_clear_tx_descs(e1000);
        __e1000_flush_packet_queue(e1000);
    }
}

static void e1000_interrupt(struct irq_frame *frame, void *param)
{
    struct net_interface_e1000 *e1000 = param;

    uint32_t icr = e1000_command_read32(e1000, REG_ICR);
    e1000_command_write32(e1000, REG_ICR, icr);

    if (icr & (ICR_RXT0 | ICR_LSC)) {
        using_spinlock(&e1000->rx_lock)
            __e1000_rx_interrupt(e1000);
    }

    /* We turn on the TXDW interrupt, but we check and try to queue packets
     * even if it isn't a TXDW */
    using_spinlock(&e1000->net.tx_lock) {
        __e1000_clear_tx_descs(e1000);
        __e1000_flush_packet_queue(e1000);
    }
}

static void e1000_setup_rx(struct net_interface_e1000 *e1000)
{
    e1000->rx_descs = kzalloc(sizeof(*e1000->rx_descs) * E1000_NUM_RX_DESC, PAL_KERNEL);
    e1000->cur_rx = 0;

    int i;
    for (i = 0; i < E1000_NUM_RX_DESC; i++) {
        /* Each recieve descriptor gets a single page */
        e1000->rx_descs[i].addr = palloc_pa(0, PAL_KERNEL);
        e1000->rx_descs[i].status = 0;
    }

    e1000_command_write32(e1000, REG_RDBAH, 0);
    e1000_command_write32(e1000, REG_RDBAL, V2P(e1000->rx_descs));

    e1000_command_write32(e1000, REG_RDLEN, E1000_NUM_RX_DESC * sizeof(*e1000->rx_descs));

    e1000_command_write32(e1000, REG_RDH, 0);
    e1000_command_write32(e1000, REG_RDT, E1000_NUM_RX_DESC - 1);

    e1000_command_write32(e1000, REG_RCTRL, RCTL_EN
                                          | RCTL_BAM
                                          | RCTL_SECRC
                                          | RCTL_BSIZE_4096
                                          );
}

static void e1000_setup_tx(struct net_interface_e1000 *e1000)
{
    e1000->tx_descs = kzalloc(sizeof(*e1000->tx_descs) * E1000_NUM_TX_DESC, PAL_KERNEL);
    e1000->last_clear = E1000_NUM_TX_DESC - 1;
    e1000->next_tx = 0;

    e1000_command_write32(e1000, REG_TDBAH, 0);
    e1000_command_write32(e1000, REG_TDBAL, V2P(e1000->tx_descs));

    e1000_command_write32(e1000, REG_TDLEN, E1000_NUM_TX_DESC * sizeof(*e1000->tx_descs));

    e1000_command_write32(e1000, REG_TDH, 0);
    e1000_command_write32(e1000, REG_TDT, 0);

    e1000_command_write32(e1000, REG_TCTRL, TCTL_EN
                                          | TCTL_PSP
                                          | (0x10 << TCTL_CT_SHIFT)
                                          );
}

void e1000_device_init(struct pci_dev *dev)
{
    struct net_interface_e1000 *e1000 = kzalloc(sizeof(*e1000), PAL_KERNEL);
    uint16_t command_reg;

    net_interface_init(&e1000->net);
    spinlock_init(&e1000->rx_lock);

    kp(KP_NORMAL, "Found Intel E1000 NIC: "PRpci_dev"\n", Ppci_dev(dev));

    e1000->net.name = "eth";
    e1000->net.process_tx_queue = e1000_process_tx_queue;
    e1000->net.linklayer_tx = arp_tx;

    e1000->dev = *dev;

    /* Enable BUS Mastering and I/O Space*/
    command_reg = pci_config_read_uint16(dev, PCI_COMMAND);
    pci_config_write_uint16(dev, PCI_COMMAND, command_reg | PCI_COMMAND_BUS_MASTER | PCI_COMMAND_MEM_SPACE);

    if (pci_config_read_uint32(dev, PCI_REG_BAR(0)) & PCI_BAR_IO) {
        e1000->io_base = pci_config_read_uint32(dev, PCI_REG_BAR(0)) & 0xFFF8;
    } else {
        uint32_t mem = pci_config_read_uint32(dev, PCI_REG_BAR(0)) & 0xFFFFFFF0;
        uint32_t size = pci_bar_size(dev, PCI_REG_BAR(0));

        e1000->mem_base = kmmap(mem, size, F(VM_MAP_READ) | F(VM_MAP_WRITE) | F(VM_MAP_NOCACHE));
        kp(KP_NORMAL, "  E1000 MEM: 0x%08x, %d bytes\n", mem, size);
    }

    kp(KP_NORMAL, "  E1000 IO base: 0x%04x\n", e1000->io_base);
    kp(KP_NORMAL, "  E1000 MEM base: %p\n", e1000->mem_base);

    e1000_eeprom_detect(e1000);
    e1000_read_mac(e1000);

    kp(KP_NORMAL, "  E1000 MAC: "PRmac"\n", Pmac(e1000->net.mac));

    int int_line = pci_config_read_uint8(dev, PCI_REG_INTERRUPT_LINE);
    kp(KP_NORMAL, "  Interrupt: %d\n", int_line);

    e1000_setup_rx(e1000);
    e1000_setup_tx(e1000);

    int err = irq_register_callback(int_line, e1000_interrupt, "E1000", IRQ_INTERRUPT, e1000, F(IRQF_SHARED));
    if (err) {
        kp(KP_WARNING, "e1000: Interrupt %d already taken and not shared!\n", PIC8259_IRQ0 + int_line);
        return;
    }

    e1000_command_write32(e1000, REG_IMS, IMS_RXT0
                                        | IMS_TXDW
                                        | IMS_RXDMT0
                                        | IMS_RXSEQ
                                        | IMS_LSC
                                        );

    e1000_command_write32(e1000, REG_CTRL, ECTRL_SLU | ECTRL_FD);

    net_interface_register(&e1000->net);
}

