/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/stdarg.h>
#include <protura/spinlock.h>
#include <protura/mm/palloc.h>
#include <protura/fs/file.h>
#include <protura/fs/poll.h>
#include <protura/fs/seq_file.h>
#include <protura/wait.h>
#include <protura/char_buf.h>
#include <protura/basic_printf.h>
#include <protura/debug.h>
#include <protura/drivers/com.h>

static char klog_buffer[(1 << CONFIG_KLOG_BUFFER_ORDER) * PG_SIZE];

struct {
    spinlock_t lock;
    struct wait_queue has_pending;
    struct char_buf buf;
} klog = {
    .lock = SPINLOCK_INIT("klog-lock"),
    .has_pending = WAIT_QUEUE_INIT(klog.has_pending, "klog-has-pending"),
    .buf = {
        .buffer = klog_buffer,
        .len = sizeof(klog_buffer),
    },
};

static void __klog_putchar(struct printf_backbone *backbone, char ch)
{
    char_buf_write_char(&klog.buf, ch);
}

static void __klog_putnstr(struct printf_backbone *backbone, const char *str, size_t len)
{
    char_buf_write(&klog.buf, str, len);
}

static struct printf_backbone klog_backbone = {
    .putchar = __klog_putchar,
    .putnstr = __klog_putnstr,
};

static void klog_printf(struct kp_output *out, const char *fmt, va_list lst)
{
    scoped_spinlock(&klog.lock) {
        basic_printfv(&klog_backbone, fmt, lst);
        wait_queue_wake(&klog.has_pending);
    }
}

static struct kp_output klog_kp_output
    = KP_OUTPUT_INIT(klog_kp_output, klog_printf, "klog");

void klog_init(void)
{
    kp_output_register(&klog_kp_output);
}

static int klog_read(struct file *filp, void *p, size_t size)
{
    scoped_spinlock(&klog.lock)
        return char_buf_read(&klog.buf, p, size);
}

static int klog_poll(struct file *filp, struct poll_table *table, int events)
{
    int ret = 0;

    scoped_spinlock(&klog.lock) {
        if (flag_test(&filp->flags, FILE_READABLE) && events & POLLIN) {
            if (char_buf_has_data(&klog.buf))
                ret |= POLLIN;
            else
                poll_table_add(table, &klog.has_pending);
        }
    }

    return ret;
}

const struct file_ops klog_file_ops = {
    .read = klog_read,
    .poll = klog_poll,
};
