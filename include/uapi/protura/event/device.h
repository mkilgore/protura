#ifndef INCLUDE_UAPI_PROTURA_EVENT_DEVICE_H
#define INCLUDE_UAPI_PROTURA_EVENT_DEVICE_H

/* Reports device add/removal events from the kernel.
 *
 * The 'code' field indicates the kind of event.
 * The 'value' field is the dev_t for the device. */

enum {
    KERN_EVENT_DEVICE_ADD,
    KERN_EVENT_DEVICE_REMOVE,

    /* If the top bit is set, this device is a block device.
     * If it isn't set, it is a char device. */
    KERN_EVENT_DEVICE_IS_BLOCK = (1 << 15),
};

#endif
