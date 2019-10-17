/*
 * Copyright (C) 2017 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

/*
 * Note: Not currently working. See RTL8139 driver for a functional network card driver.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/kmmap.h>
#include <protura/snprintf.h>
#include <arch/asm.h>

#include <protura/fs/procfs.h>
#include <protura/drivers/pci.h>
#include <protura/drivers/pci_ids.h>
#include <protura/net/arp.h>
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
        e1000->mac[0] = tmp & 0xFF;
        e1000->mac[1] = tmp >> 8;

        tmp = e1000_eeprom_read(e1000, 2);
        e1000->mac[2] = tmp & 0xFF;
        e1000->mac[3] = tmp >> 8;

        tmp = e1000_eeprom_read(e1000, 4);
        e1000->mac[4] = tmp & 0xFF;
        e1000->mac[5] = tmp >> 8;

    } else {
        int i;
        for (i = 0; i < 6; i++) {
            e1000->mac[i] = ((uint8_t *)e1000->mem_base)[0x5400 + i];
        }
    }

    kp(KP_ERROR, "E1000 MAC: "PRmac"\n", Pmac(e1000->mac));
}

static void e1000_packet_send(struct net_interface *net, struct packet *packet)
{
}

void e1000_device_init(struct pci_dev *dev)
{
    struct net_interface_e1000 *e1000 = kzalloc(sizeof(*e1000), PAL_KERNEL);
    uint16_t command_reg;

    net_interface_init(&e1000->net);

    kp(KP_NORMAL, "Found Intel E1000 NIC: "PRpci_dev"\n", Ppci_dev(dev));

    e1000->net.name = "eth";
    e1000->net.hard_tx = e1000_packet_send;
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

        e1000->mem_base = kmmap(mem, size, F(VM_MAP_READ) | F(VM_MAP_WRITE) | F(VM_MAP_NOCACHE) | F(VM_MAP_WRITETHROUGH));
        kp(KP_NORMAL, "  E1000 MEM: 0x%08x\n", mem);
        kp(KP_NORMAL, "  E1000 MEM size: %d\n", size);
    }

    kp(KP_NORMAL, "  E1000 IO base: 0x%04x\n", e1000->io_base);
    kp(KP_NORMAL, "  E1000 MEM base: %p\n", e1000->mem_base);

    e1000_eeprom_detect(e1000);
    e1000_read_mac(e1000);

    net_interface_register(&e1000->net);
}

