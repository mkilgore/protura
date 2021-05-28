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
#include <protura/scheduler.h>
#include <protura/drivers/pci.h>
#include <protura/irq.h>
#include <protura/io.h>
#include <protura/usb/usb.h>

static spinlock_t usb_list_lock = SPINLOCK_INIT();

static list_head_t usb_bus_list = LIST_HEAD_INIT(usb_bus_list);
static list_head_t usb_device_list = LIST_HEAD_INIT(usb_device_list);

void usb_bus_register(struct usb_bus *bus)
{
    using_spinlock(&usb_list_lock)
        list_add_tail(&usb_bus_list, &bus->entry);
}

void usb_bus_unregister(struct usb_bus *bus)
{
    using_spinlock(&usb_list_lock)
        list_del(&bus->entry);
}

struct usb_device *usb_device_alloc(struct usb_bus *bus)
{
    struct usb_device *dev = (bus->ops->dev_alloc) (bus);
    if (!dev)
        return NULL;

    dev->bus = bus;
    return dev;
}

struct usb_device *usb_device_dup(struct usb_device *device)
{
    using_spinlock(&usb_list_lock)
        device->refs++;

    return device;
}

void usb_device_put(struct usb_device *device)
{
    using_spinlock(&usb_list_lock)
        device->refs--;
}

/* FIXME */
void usb_device_add(struct usb_device *device)
{
    int ret = urb_do_ctrl(device,
                usb_pipe_make(USB_PIPE_CONTROL, USB_PIPE_IN, device->dev, 0),
                USB_REQ_GET_DESCRIPTOR,
                USB_CTRL_RECIP_DEVICE | USB_REQ_DIR_IN,
                USB_DT_DEVICE << 8,
                0,
                &device->dev_desc,
                sizeof(device->dev_desc));

    kp(KP_NORMAL, "ohci: get descriptor: ret: %d\n", ret);
    if (ret == sizeof(device->dev_desc)) {
        struct usb_device_descriptor *desc = &device->dev_desc;

        kp(KP_NORMAL, "ohci: bLength: %d, bDescriptorType: %d\n", desc->header.bLength, desc->header.bDescriptorType);

        if (desc->header.bLength == 18 && desc->header.bDescriptorType == 0x01) {
            kp(KP_NORMAL, "ohci: dev: bcdUSB: 0x%04x, bDeviceClass: %d, bDeviceSubClass: %d\n", desc->bcdUSB, desc->bDeviceClass, desc->bDeviceSubClass);
            kp(KP_NORMAL, "ohci: dev: bDeviceProtocol: %d, bMaxPacketSize: %d\n", desc->bDeviceProtocol, desc->bMaxPacketSize0);
            kp(KP_NORMAL, "ohci: dev: idVendor: 0x%04x, idProduct: 0x%04x, bcdDevice: 0x%04x\n", desc->idVendor, desc->idProduct, desc->bcdDevice);
            kp(KP_NORMAL, "ohci: dev: iManufacturer: 0x%2x, iProduct: 0x%02x, iSerialNumber: 0x%02x\n", desc->iManufacturer, desc->iProduct, desc->iSerialNumber);
            kp(KP_NORMAL, "ohci: dev: bNumConfigurations: %d\n", desc->bNumConfigurations);
        }
    }

    using_spinlock(&usb_list_lock)
        list_add_tail(&usb_device_list, &device->node);
}

void urb_complete(struct urb *urb)
{
    using_spinlock(&urb->status_lock)
        urb->status = URB_DONE;

    work_schedule(&urb->on_complete);
}

int urb_do_ctrl(struct usb_device *device, struct usb_pipe pipe, uint8_t request, uint8_t request_type, uint16_t value, uint16_t index, void *data, uint16_t data_length)
{
    struct urb *urb = urb_alloc();

    urb->device = device;
    urb->bus = device->bus;
    urb->pipe = pipe;

    struct usb_ctrl_setup *setup = kzalloc(sizeof(*setup), PAL_KERNEL);

    setup->request = request;
    setup->request_type = request_type;
    setup->value = value;
    setup->index = index;
    setup->length = data_length;

    urb->setup = setup;
    urb->data = data;
    urb->data_length = data_length;

    work_init_task(&urb->on_complete, cpu_get_local()->current);

    urb_submit(urb);

    using_spinlock(&urb->status_lock)
        sleep_event_spinlock(urb->status == URB_DONE, &urb->status_lock);

    kfree(setup);

    int err = urb->err;
    size_t transferred = urb->transferred_data;

    urb_put(urb);

    if (err)
        return err;
    else
        return transferred;
}

struct urb *urb_dup(struct urb *urb)
{
    atomic_inc(&urb->refs);
    return urb;
}

struct urb *urb_alloc(void)
{
    struct urb *urb = kzalloc(sizeof(*urb), PAL_KERNEL);
    spinlock_init(&urb->status_lock);
    return urb_dup(urb);
}

void urb_put(struct urb *urb)
{
    if (atomic_dec_and_test(&urb->refs))
        kfree(urb);
}

void urb_submit(struct urb *urb)
{
    (urb->bus->ops->urb_submit) (urb->bus, urb);
}
