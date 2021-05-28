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
#include <protura/task.h>
#include <protura/drivers/pci.h>
#include <protura/irq.h>
#include <protura/io.h>

#include "ohci.h"

static struct ohci_ed *ohci_ed_alloc(void)
{
    struct ohci_ed *ed = kzalloc(sizeof(*ed), PAL_KERNEL | PAL_ATOMIC);
    list_node_init(&ed->ed_node);

    return ed;
}

static struct ohci_td *ohci_td_alloc(void)
{
    struct ohci_td *td = kzalloc(sizeof(*td), PAL_KERNEL | PAL_ATOMIC);

    return td;
}

static struct ohci_ed *ohci_ed_create(void)
{
    struct ohci_ed *ed = ohci_ed_alloc();

    ed->dummy = ohci_td_alloc();

    ed->hw.HeadP = V2P(ed->dummy);
    ed->hw.TailP = ed->hw.HeadP;

    return ed;
}

static void __ohci_ed_schedule(struct ohci_bus *bus, struct ohci_ed *ed)
{
    list_del(&ed->ed_node);
    ed->hw.NextED = 0;

    switch (ed->type) {
    case USB_PIPE_CONTROL:
        if (list_empty(&bus->ed_list)) {
            write32(&bus->regs->HcControlHeadED, V2P(ed));
        } else {
            list_last(&bus->ed_list, struct ohci_ed, ed_node)->hw.NextED = V2P(ed);
        }

        /* Kick off the control list if it was empty */
        if (list_empty(&bus->ed_list) && !(bus->ctrl_copy & OHCI_CTRL_CLE)) {
            bus->ctrl_copy |= OHCI_CTRL_CLE;
            write32(&bus->regs->HcControlCurrentED, 0);
            write32(&bus->regs->HcControl, bus->ctrl_copy);
        }

        list_add_tail(&bus->ed_list, &ed->ed_node);
        break;
    }
}

static void ohci_ed_set_info(struct ohci_ed *ed, struct usb_pipe pipe)
{
    ed->hw.info = pipe.dev;
    ed->hw.info |= pipe.ep << 7;
    ed->hw.info |= 64 << 16;

    ed->type = pipe.type;

    switch (pipe.type) {
    case USB_PIPE_CONTROL:
        ed->hw.info &= ~OHCI_ED_ISO;
        /* Control transfers determine direction from the TD, so we don't set
         * the direction here */
        break;
    }
}

static struct usb_device *ohci_dev_alloc(struct usb_bus *bus)
{
    struct ohci_usb_device *dev = kzalloc(sizeof(*dev), PAL_KERNEL);
    usb_device_init(&dev->device);

    return &dev->device;
}

static void ohci_dev_free(struct usb_bus *bus, struct usb_device *device)
{
    struct ohci_usb_device *dev = container_of(device, struct ohci_usb_device, device);
    kfree(dev);
}

static void ohci_td_fill(struct ohci_ed *ed, struct urb *urb, uint32_t info, va_t data, int data_len)
{
    struct ohci_urb_priv *priv = urb->priv;
    struct ohci_td *next_dummy = priv->tds[priv->next_td++];
    struct ohci_td *cur_td = ed->dummy;

    ed->dummy = next_dummy;

    priv->tds[priv->next_td - 1] = cur_td;

    cur_td->ed = ed;
    cur_td->urb = urb;
    cur_td->urb_index = priv->next_td - 1;

    cur_td->hw.info = info;
    cur_td->hw.CBP = V2P(data);
    cur_td->buffer = data;
    cur_td->buffer_len = data_len;

    if (data && data_len)
        cur_td->hw.BE = V2P(data) + data_len - 1;
    else
        cur_td->hw.BE = 0;

    cur_td->hw.NextTD = V2P(next_dummy);

    /* Tell the USB controller to process the new TD */
    ed->hw.TailP = V2P(next_dummy);
}

static struct ohci_ed *ohci_ed_get(struct ohci_bus *bus, struct urb *urb)
{
    if (urb->pipe.ep == 0 && urb->pipe.dev == 0)
        return bus->ed0;

    int ep = urb->pipe.ep;
    struct ohci_usb_device *device = container_of(urb->device, struct ohci_usb_device, device);

    switch (urb->pipe.dir) {
    case USB_PIPE_IN:
        if (!device->ineds[ep]) {
            device->ineds[ep] = ohci_ed_create();
            ohci_ed_set_info(device->ineds[ep], urb->pipe);

            __ohci_ed_schedule(bus, device->ineds[ep]);
        }

        return device->ineds[ep];

    case USB_PIPE_OUT:
        if (!device->outeds[ep]) {
            device->outeds[ep] = ohci_ed_create();
            ohci_ed_set_info(device->outeds[ep], urb->pipe);

            __ohci_ed_schedule(bus, device->outeds[ep]);
        }

        return device->outeds[ep];
    }

    return NULL;
}

static void ohci_urb_create_tds(struct ohci_bus *bus, struct ohci_ed *ed, struct urb *urb)
{
    uint32_t info;
    int is_out = (urb->pipe.dir == USB_PIPE_OUT);

    kp(KP_NORMAL, "urb: %p, dir: %d, is_out: %d\n", urb, urb->pipe.dir, is_out);

    switch (urb->pipe.type) {
    case USB_PIPE_CONTROL:

        info = OHCI_TD_CC | OHCI_TD_DP_SETUP | OHCI_TD_T_DATA0;
        ohci_td_fill(ed, urb, info, urb->setup, 8);

        if (urb->data) {
            info = OHCI_TD_CC | OHCI_TD_R | OHCI_TD_T_DATA1 | (is_out? OHCI_TD_DP_OUT: OHCI_TD_DP_IN);
            if (info & OHCI_TD_DP_IN)
                kp(KP_NORMAL, "urb: %p, td IN!\n", urb);
            ohci_td_fill(ed, urb, info, urb->data, urb->data_length);
        }

        info = OHCI_TD_CC
             | OHCI_TD_T_DATA1
             | (is_out? OHCI_TD_DP_IN: OHCI_TD_DP_OUT);
        ohci_td_fill(ed, urb, info, 0, 0);

        /* Start the control if it isn't already started */
        write32(&bus->regs->HcControl, bus->ctrl_copy | OHCI_CTRL_CLE);
        write32(&bus->regs->HcCommandStatus, OHCI_CLF);
        break;
    }
}

static void ohci_urb_submit(struct usb_bus *usb_bus, struct urb *urb)
{
    struct ohci_bus *bus = container_of(usb_bus, struct ohci_bus, bus);
    int td_count = 0;

    switch (urb->pipe.type) {
    case USB_PIPE_CONTROL:
        td_count = 2;

        if (urb->data)
            td_count += (urb->data_length + 4095) / 4096;

        break;
    }

    urb = urb_dup(urb);

    /* Allocate td's into the urb private */
    struct ohci_urb_priv *priv = kzalloc(sizeof(*priv) + sizeof(*priv->tds) * td_count, PAL_KERNEL);

    priv->total_tds = td_count;
    priv->tds_left = td_count;

    urb->priv = priv;

    int i;
    for (i = 0; i < td_count; i++)
        priv->tds[i] = ohci_td_alloc();

    using_spinlock(&bus->bus.lock) {
        /* Grab the ed, creating and scheduling it if necessary */
        struct ohci_ed *ed = ohci_ed_get(bus, urb);

        /* Fill tds, replacing dummy with an entry from the priv */
        ohci_urb_create_tds(bus, ed, urb);
    }
}

struct ohci_work {
    struct work work;
    struct ohci_bus *bus;
    int port;
};

static void ohci_register_dev(struct work *work)
{
    struct ohci_work *ohciw = container_of(work, struct ohci_work, work);
    struct ohci_bus *bus = ohciw->bus;

    struct usb_device *dev = usb_device_alloc(&bus->bus);
    int new_ep = ida_getid(&bus->bus.dev_ida);

    dev->dev = new_ep;

    using_mutex(&bus->hotplug_lock) {
        kp(KP_NORMAL, "ohci: attempting reset on port %d...\n", ohciw->port);
        write32(bus->regs->HcRhPortStatus + ohciw->port, OHCI_RH_PS_PRS);

        task_sleep_ms(1000);

        kp(KP_NORMAL, "ohci: port %d: Sending ctrl set address message, id: %d.\n", ohciw->port, new_ep);
        int err = urb_do_ctrl(dev,
                    USB_PIPE_DEFAULT_CONTROL,
                    USB_REQ_SET_ADDRESS,
                    USB_CTRL_RECIP_DEVICE,
                    new_ep,
                    0,
                    NULL,
                    0);
        kp(KP_NORMAL, "ohci: set address: err: %d\n", err);

        if (err < 0) {
            kp(KP_WARNING, "ohci: Error setting address: %d\n", err);
            return;
        }
    }

    usb_device_add(dev);
    kfree(ohciw);
}

static void ohci_dump_roothub(struct ohci_bus *bus)
{
    int i;
    for (i = 0; i < bus->port_count; i++) {
        uint32_t status = read32(&bus->regs->HcRhPortStatus[i]);
        if (status & OHCI_RH_PS_CSC) {
            if (status & OHCI_RH_PS_CCS) {
                kp(KP_WARNING, "Root hub port %d connected!\n", i);
                struct ohci_work *work = kzalloc(sizeof(*work), PAL_KERNEL | PAL_ATOMIC);

                work_init_kwork(&work->work, ohci_register_dev);
                flag_set(&work->work.flags, WORK_ONESHOT);
                work->bus = bus;
                work->port = i;

                work_schedule(&work->work);
            } else {
                kp(KP_WARNING, "Root hub port %d disconnected!\n", i);
            }
        } else if (status & OHCI_RH_PS_PRSC) {
            kp(KP_WARNING, "Root hub port %d, reset complete!\n", i);
        } else if ((status >> 16)) {
            kp(KP_WARNING, "Root hub port %d, other event: 0x%08x!\n", i, status);
        }

        write32(&bus->regs->HcRhPortStatus[i], status);
    }
}

static void ohci_finish_urb(struct urb *urb)
{
    struct ohci_urb_priv *priv = urb->priv;

    kp(KP_NORMAL, "Finishing urb: %p\n", urb);

    int i;
    for (i = 0; i < priv->total_tds; i++)
        kfree(priv->tds[i]);

    kfree(priv);

    urb_complete(urb);
    urb_put(urb);
}

static void ohci_handle_td(struct ohci_td *td)
{
    int cc;
    struct urb *urb = td->urb;

    kp(KP_NORMAL, "Processing TD: %p, urb: %p, index: %d\n", td, td->urb, td->urb_index);

    switch (urb->pipe.type) {
    case USB_PIPE_CONTROL:
        if (td->urb_index != 0 && td->hw.BE != 0) {
            if (td->hw.CBP)
                urb->transferred_data += td->hw.CBP - V2P(td->buffer);
            else
                urb->transferred_data += td->buffer_len;
        }

        cc = OHCI_TD_CC_GET(td->hw.info);

        if (cc)
            urb->err = -EIO;

        break;
    }
}

static const char *cc_to_str[16] = {
    "no error",
    "crc",
    "bit stuffing",
    "data toggle em",
    "stall",
    "dev not responding",
    "pid check fail",
    "unexpected pid",
    "data overrun",
    "data underrun",
    "not accessed",
    "not accessed",
    "buffer overrun",
    "buffer underrun",
    "not accessed",
    "not accessed",
};

static void ohci_irq_handler(struct irq_frame *frame, void *param)
{
    struct ohci_bus *bus = param;

    using_spinlock(&bus->bus.lock) {
        /* Read and clear interrupts */
        uint32_t status = read32(&bus->regs->HcInterruptStatus);

        kp(KP_NORMAL, "OHCI interrupt! status: 0x%08x, ue: %d\n", status, status & OHCI_INTR_UE);

        if (status & OHCI_INTR_WDH) {
            struct ohci_td *prev_td = NULL, *next_td = NULL, *td = NULL;
            pa_t td_pa = bus->hcca->DoneHead & 0xFFFFFFFE;
            bus->hcca->DoneHead = 0;

            /* Reverse the done_head list, since it's backwards from the order the tds were processed */
            for (; td_pa; td_pa = td->hw.NextTD) {
                td = P2V(td_pa);

                td->rev_list_next = prev_td;
                prev_td = td;
            }

            if (td) {
                for (next_td = td->rev_list_next; td; td = next_td, next_td = next_td? next_td->rev_list_next: NULL) {
                    struct urb *urb = td->urb;
                    struct ohci_urb_priv *priv = urb->priv;

                    ohci_handle_td(td);
                    priv->tds_left--;

                    if (priv->tds_left == 0)
                        ohci_finish_urb(urb);
                }
            }
        }

        ohci_dump_roothub(param);

        write32(&bus->regs->HcInterruptStatus, status);
    }
}

static struct usb_bus_ops ohci_bus_ops = {
    .dev_alloc = ohci_dev_alloc,
    .dev_free = ohci_dev_free,
    .urb_submit = ohci_urb_submit,
};

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
    bus->bus.ops = &ohci_bus_ops;

    bus->regs = kmmap(mem, size, F(VM_MAP_READ, VM_MAP_WRITE));
    kp(KP_NORMAL, "  OHCI Revision: %d\n", bus->regs->HcRevision);

    if (read32(&bus->regs->HcControl) & OHCI_CTRL_IR) {
        kp(KP_NORMAL, "Owned by system!\n");

        write32(&bus->regs->HcInterruptEnable, OHCI_INTR_OC);
        write32(&bus->regs->HcCommandStatus, OHCI_OCR);

        while (read32(&bus->regs->HcControl) & OHCI_CTRL_IR) {
            kp(KP_NORMAL, "Trying to take over ohci controller...\n");
            task_sleep_ms(1);
        }
    }

    struct ohci_ed *ed = ohci_ed_create();

    /* The 0 device is used for assigning an address */
    ohci_ed_set_info(ed, usb_pipe_make(USB_PIPE_CONTROL, USB_PIPE_OUT, 0, 0));
    bus->ed0 = ed;

    int err = irq_register_callback(irq, ohci_irq_handler, "OHCI", IRQ_INTERRUPT, bus, F(IRQF_SHARED));
    if (err)
        kp(KP_WARNING, "OHCI: Unable to acquire interrupt %d!\n", irq);

    write32(&bus->regs->HcInterruptDisable, 0xFFFFFFFF);
    write32(&bus->regs->HcCommandStatus, OHCI_HCR);

    bus->port_count = read32(&bus->regs->HcRhDescriptorA) & 0xF;
    kp(KP_NORMAL, "  OHCI Total ports: %d\n", bus->port_count);

    /* Per Linux, default values for fm_interval */
    write32(&bus->regs->HcFmInterval, (((6 * (FI - 210)) / 7) << 16) | FI);
	write32(&bus->regs->HcPeriodicStart, ((9 * FI) / 10) & 0x3fff);
	write32(&bus->regs->HcLSThreshold, LSTHRESH);

    write32(&bus->regs->HcControlHeadED, 0);
    write32(&bus->regs->HcControlCurrentED, 0);
    write32(&bus->regs->HcBulkHeadED, 0);
    write32(&bus->regs->HcBulkCurrentED, 0);
    write32(&bus->regs->HcPeriodCurrentED, 0);

    bus->ctrl_copy = OHCI_CTRL_CBSR | OHCI_CTRL_RWC | OHCI_USB_OPER | OHCI_CTRL_CLE;
    write32(&bus->regs->HcControl, bus->ctrl_copy);

    write32(&bus->regs->HcInterruptEnable, OHCI_INTR_RHSC | OHCI_INTR_MIE | OHCI_INTR_WDH);

    bus->hcca = kzalloc(sizeof(*bus->hcca), PAL_KERNEL);
    write32(&bus->regs->HcHCCA, V2P(bus->hcca));

    __ohci_ed_schedule(bus, bus->ed0);

    ohci_dump_roothub(bus);
}
