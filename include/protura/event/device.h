#ifndef INCLUDE_PROTURA_EVENT_DEVICE_H
#define INCLUDE_PROTURA_EVENT_DEVICE_H

#include <uapi/protura/event/device.h>
#include <protura/fs/file.h>
#include <protura/types.h>

void device_event_submit(int device_event, int is_block, dev_t);

static inline void device_submit_block(int device_event, dev_t dev)
{
    device_event_submit(device_event, 1, dev);
}

static inline void device_submit_char(int device_event, dev_t dev)
{
    device_event_submit(device_event, 0, dev);
}

extern struct file_ops device_event_file_ops;

#endif
