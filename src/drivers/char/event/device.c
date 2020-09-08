/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/dev.h>
#include <protura/string.h>
#include <protura/fs/char.h>
#include <protura/fs/file.h>
#include <protura/event/device.h>
#include <protura/event/protocol.h>

static struct kern_event device_event_queue_buffer[256];

static struct event_queue device_event_queue
    = EVENT_QUEUE_INIT_FLAGS(device_event_queue, device_event_queue_buffer, F(EQUEUE_FLAG_BUFFER_EVENTS));

static const char *device_event_strs[] = {
    [KERN_EVENT_DEVICE_ADD] = "add",
    [KERN_EVENT_DEVICE_REMOVE] = "remove",
};

void device_event_submit(int device_event, int is_block, dev_t dev)
{
    const char *str = "unknown!";
    if (device_event < ARRAY_SIZE(device_event_strs))
        str = device_event_strs[device_event];

    kp(KP_NORMAL, "device: (%c, %d, %d): %s\n",
            is_block? 'b': 'c',
            DEV_MAJOR(dev), DEV_MINOR(dev),
            str);

    event_queue_submit_event(&device_event_queue,
                             KERN_EVENT_DEVICE,
                             device_event | (is_block? KERN_EVENT_DEVICE_IS_BLOCK: 0),
                             dev);
}

static int device_event_open(struct inode *inode, struct file *filp)
{
    return event_queue_open(filp, &device_event_queue);
}

struct file_ops device_event_file_ops = {
    .open = device_event_open,
    .read = event_queue_read,
    .release = event_queue_release,
};
