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
#include <protura/fs/char.h>
#include <protura/fs/file.h>
#include <protura/scheduler.h>
#include <protura/wait.h>
#include <protura/dev.h>
#include <protura/initcall.h>
#include <protura/mm/user_check.h>
#include <protura/event/device.h>
#include <protura/event/keyboard.h>
#include <protura/event/protocol.h>

static inline size_t next(size_t val, size_t size)
{
    return (val + 1) % size;
}

static inline int queue_full(struct event_queue *queue)
{
    return next(queue->tail, queue->size) == queue->head;
}

static inline int queue_empty(struct event_queue *queue)
{
    return queue->tail == queue->head;
}

static void event_queue_drop_reader(struct event_queue *queue)
{
    using_spinlock(&queue->lock) {
        queue->open_readers--;

        if (queue->open_readers == 0 && !flag_test(&queue->flags, EQUEUE_FLAG_BUFFER_EVENTS))
            queue->tail = queue->head;
    }
}

void event_queue_submit_event(struct event_queue *queue, uint16_t type, uint16_t code, uint32_t value)
{
    using_spinlock(&queue->lock) {
        if ((queue->open_readers || flag_test(&queue->flags, EQUEUE_FLAG_BUFFER_EVENTS)) && !queue_full(queue)) {
            queue->buffer[queue->tail].type = type;
            queue->buffer[queue->tail].code = code;
            queue->buffer[queue->tail].value = value;

            queue->tail = next(queue->tail, queue->size);
        }
    }

    wait_queue_wake(&queue->event_wait);
}

int event_queue_open(struct file *filp, struct event_queue *queue)
{
    filp->priv_data = queue;

    using_spinlock(&queue->lock)
        queue->open_readers++;

    return 0;
}

int event_queue_read(struct file *filp, struct user_buffer buf, size_t size)
{
    struct event_queue *queue = filp->priv_data;
    int err = 0;
    size_t index = 0;
    size_t max_events = size / sizeof(struct kern_event);
    int should_wait = !flag_test(&filp->flags, FILE_NONBLOCK);
    struct kern_event tmp_event;

    if (max_events == 0)
        return -EINVAL;

  again:
    spinlock_acquire(&queue->lock);
  again_locked:

    if (index != max_events && !queue_empty(queue)) {
        tmp_event = queue->buffer[queue->head];
        queue->head = next(queue->head, queue->size);

        max_events--;
        should_wait = 0;

        spinlock_release(&queue->lock);

        err = user_copy_from_kernel_indexed(buf, tmp_event, index);
        if (err)
            return err;

        index++;

        goto again;
    } else if (should_wait) {
        err = wait_queue_event_intr_spinlock(&queue->event_wait, !queue_empty(queue), &queue->lock);
        if (err) {
            spinlock_release(&queue->lock);
            return err;
        }

        goto again_locked;
    }
    spinlock_release(&queue->lock);

    /* We never return EOF, since the stream is infinite. The case of index==0
     * only happens if FILE_NONBLOCK was set on the file */
    if (index == 0)
        return -EAGAIN;

    return index * sizeof(struct kern_event);
}

int event_queue_release(struct file *filp)
{
    struct event_queue *queue = filp->priv_data;

    event_queue_drop_reader(queue);
    return 0;
}

int event_open(struct inode *inode, struct file *filp)
{
    dev_t minor = DEV_MINOR(inode->dev_no);

    switch (minor) {
    case EVENT_MINOR_KEYBOARD:
        filp->ops = &keyboard_event_file_ops;
        return (filp->ops->open) (inode, filp);
    }

    return -ENODEV;
}

struct file_ops event_file_ops = {
    .open = event_open,
};

static void event_device_init(void)
{
    device_submit_char(KERN_EVENT_DEVICE_ADD, DEV_MAKE(CHAR_DEV_EVENT, EVENT_MINOR_KEYBOARD));
}
initcall_device(event_device, event_device_init);
