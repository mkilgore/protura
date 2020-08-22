/*
 * Copyright (C) 2020 Matt Kilgore
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
#include <protura/drivers/pci.h>
#include <protura/irq.h>
#include <protura/io.h>

#include "ohci.h"

static void ohci_dump_roothub(struct ohci_bus *bus)
{
    int i;
    for (i = 0; i < bus->port_count; i++) {
        uint32_t status = read32(&bus->regs->roothub_port_status[i]);
        if (status & RH_PS_CSC) {
            if (status & RH_PS_CCS)
                kp(KP_WARNING, "Root hub port %d connected!\n", i);
            else
                kp(KP_WARNING, "Root hub port %d disconnected!\n", i);

            /* Clear 'connect status change' flag */
            write32(&bus->regs->roothub_port_status[i], RH_PS_CSC);
        } else if ((status >> 16)) {
            kp(KP_WARNING, "Root hub port %d, other event: 0x%08x!\n", i, status);
            write32(&bus->regs->roothub_port_status[i], status);
        }
    }
}

static void ohci_irq_handler(struct irq_frame *frame, void *param)
{
    struct ohci_bus *bus = param;

    /* Read and clear interrupts */
    uint32_t status = read32(&bus->regs->interrupt_status);
    write32(&bus->regs->interrupt_status, status);

    kp(KP_NORMAL, "OHCI interrupt! status: 0x%08x\n", status);

    ohci_dump_roothub(param);
}

#define	FI		0x2edf		/* 12000 bits per frame (-1) */
#define LSTHRESH	0x628		/* lowspeed bit threshold */

void ohci_pci_init(struct pci_dev *dev)
{
    kp(KP_NORMAL, "Found OHCI USB Host Controller: "PRpci_dev"\n", Ppci_dev(dev));
    uint32_t mem = pci_config_read_uint32(dev, PCI_REG_BAR(0)) & 0xFFFFFFF0;
    size_t size = pci_bar_size(dev, PCI_REG_BAR(0));
    int irq = pci_config_read_uint8(dev, PCI_REG_INTERRUPT_LINE);

    kp(KP_NORMAL, "  MEM: 0x%08x, %d bytes\n", mem, size);

    struct ohci_bus *bus = kmalloc(sizeof(*bus), PAL_KERNEL);
    ohci_bus_init(bus);

    bus->regs = kmmap(mem, size, F(VM_MAP_READ, VM_MAP_WRITE));
    kp(KP_NORMAL, "  OHCI Revision: %d\n", bus->regs->revision);

    int err = irq_register_callback(irq, ohci_irq_handler, "OHCI", IRQ_INTERRUPT, bus, F(IRQF_SHARED));
    if (err)
        kp(KP_WARNING, "OHCI: Unable to acquire interrupt %d!\n", irq);

    bus->port_count = read32(&bus->regs->roothub_descriptor_a) & 0xF;
    kp(KP_NORMAL, "  OHCI Total ports: %d\n", bus->port_count);

    write32(&bus->regs->interrupt_disable, 0xFFFFFFFF);
    ohci_dump_roothub(bus);

    bus->hcca = kzalloc(sizeof(*bus->hcaa), PAL_KERNEL);

    write32(&bus->regs->hcca, V2P(bus->hcca));

    /* Per Linux, default values for fm_interval */
    write32(&bus->regs->fm_interval, (((6 * (FI - 210)) / 7) << 16) | FI);
	write32(&bus->regs->periodic_start, ((9 * FI) / 10) & 0x3fff);
	write32(&bus->regs->ls_threshold, LSTHRESH);

    write32(&bus->regs->control, (OHCI_CTRL_CBSR & 0X03) | OHCI_CTRL_CLE | OHCI_CTRL_RWC | OHCI_USB_OPER);

    write32(&bus->regs->interrupt_enable, OHCI_INTR_RHSC | OHCI_INTR_MIE);
}
