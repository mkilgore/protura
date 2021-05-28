#ifndef INCLUDE_PROTURA_USB_USB_H
#define INCLUDE_PROTURA_USB_USB_H

#include <protura/types.h>
#include <protura/atomic.h>
#include <protura/list.h>
#include <protura/ida.h>
#include <arch/spinlock.h>
#include <protura/usb/usb_descriptors.h>

struct usb_bus;
struct usb_device;
struct urb;

struct usb_bus_ops {
    void (*release) (struct usb_bus *);

    struct usb_device *(*dev_alloc) (struct usb_bus *);
    void (*dev_free) (struct usb_bus *, struct usb_device *);

    void (*urb_submit) (struct usb_bus *, struct urb *);
};

struct usb_bus {
    list_node_t entry;

    spinlock_t lock;
    int refs;

    uint32_t dev_ida_int;
    struct ida dev_ida;

    const struct usb_bus_ops *ops;
};

#define USB_BUS_INIT(bus) \
    { \
        .entry = LIST_NODE_INIT((bus).entry), \
        .lock = SPINLOCK_INIT(), \
        .dev_ida_int = 1, \
        .dev_ida = IDA_INIT(&(bus).dev_ida_int, 128), \
    }

static inline void usb_bus_init(struct usb_bus *bus)
{
    *bus = (struct usb_bus)USB_BUS_INIT(*bus);
}

void usb_bus_register(struct usb_bus *);
void usb_bus_unregister(struct usb_bus *);

enum usb_pipe_type {
    USB_PIPE_CONTROL,
};

enum usb_pipe_dir {
    USB_PIPE_OUT,
    USB_PIPE_IN,
};

struct usb_pipe {
    uint32_t dev :7;
    uint32_t dir :1;
    uint32_t ep :4;
    uint32_t type :2;
};

static inline struct usb_pipe usb_pipe_make(int type, int dir, int dev, int ep)
{
    return (struct usb_pipe) { .type = type, .dir = dir, .dev = dev, .ep = ep };
}

#define USB_PIPE_DEFAULT_CONTROL usb_pipe_make(USB_PIPE_CONTROL, USB_PIPE_OUT, 0, 0)

struct usb_device {
    struct usb_bus *bus;

    spinlock_t lock;
    int refs;

    /* USB device number */
    int dev;

    list_head_t node;

    struct usb_device_descriptor dev_desc;
};

#define USB_DEVICE_INIT(dev) \
    { \
        .lock = SPINLOCK_INIT(), \
        .refs = 1, \
        .node = LIST_NODE_INIT((dev).node), \
    }

static inline void usb_device_init(struct usb_device *dev)
{
    *dev = (struct usb_device)USB_DEVICE_INIT(*dev);
}

struct usb_device *usb_device_alloc(struct usb_bus *);
struct usb_device *usb_device_dup(struct usb_device *);
void usb_device_put(struct usb_device *);

void usb_device_add(struct usb_device *);

enum urb_status {
    URB_PROCESSING,
    URB_DONE,
};

struct urb {
    atomic_t refs;

    struct usb_pipe pipe;
    struct usb_bus *bus;
    struct usb_device *device;

    void *setup;
    void *data;
    size_t data_length;
    size_t transferred_data; /* Set when urb is processed */

    struct work on_complete;

    spinlock_t status_lock;
    enum urb_status status;
    int err;

    void *priv;
};

struct urb *urb_alloc(void);
struct urb *urb_dup(struct urb *);
void urb_put(struct urb *);

void urb_complete(struct urb *);

void urb_submit(struct urb *);
void urb_submit_ctrl(struct usb_device *, struct usb_pipe pipe, uint8_t request, uint8_t request_type, uint16_t value, uint16_t index, void *data, uint16_t data_length);

int urb_do_ctrl(struct usb_device *, struct usb_pipe pipe, uint8_t request, uint8_t request_type, uint16_t value, uint16_t index, void *data, uint16_t data_length);

#endif
