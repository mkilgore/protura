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
#include <protura/fs/char.h>
#include <protura/fs/file.h>
#include <protura/event/keyboard.h>
#include <protura/event/protocol.h>

static struct kern_event keyboard_event_queue_buffer[128];

static char keyboard_bitmap[KS_MAX / 8];

static struct event_queue keyboard_event_queue = EVENT_QUEUE_INIT(keyboard_event_queue, keyboard_event_queue_buffer);

void keyboard_event_queue_submit(int keysym, int is_release)
{
    /* The bitmap is used to avoid repeat events */
    if (bit_test(keyboard_bitmap, keysym) == !is_release)
        return;

    if (is_release)
        bit_clear(keyboard_bitmap, keysym);
    else
        bit_set(keyboard_bitmap, keysym);

    event_queue_submit_event(&keyboard_event_queue,
                             KERN_EVENT_KEYBOARD,
                             keysym,
                             is_release? KERN_EVENT_KEY_RELEASE: KERN_EVENT_KEY_PRESS);
}

static int keyboard_event_open(struct inode *inode, struct file *filp)
{
    return event_queue_open(filp, &keyboard_event_queue);
}

struct file_ops keyboard_event_file_ops = {
    .open = keyboard_event_open,
    .read = event_queue_read,
    .release = event_queue_release,
};

