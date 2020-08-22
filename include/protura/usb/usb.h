#ifndef INCLUDE_PROTURA_USB_USB_H
#define INCLUDE_PROTURA_USB_USB_H

#include <protura/types.h>
#include <protura/list.h>
#include <arch/spinlock.h>

struct usb_bus {
    list_node_t entry;

    spinlock_t lock;
    int refs;

    void (*release) (struct usb_bus *);
};

#define USB_BUS_INIT(bus) \
    { \
        .entry = LIST_NODE_INIT((bus).entry), \
        .lock = SPINLOCK_INIT(), \
    }

static inline void usb_bus_init(struct usb_bus *bus)
{
    *bus = (struct usb_bus)USB_BUS_INIT(*bus);
}

void usb_bus_register(struct usb_bus *);
void usb_bus_unregister(struct usb_bus *);

#endif
