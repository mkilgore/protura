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
#include <protura/mm/user_check.h>
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
    .lock = SPINLOCK_INIT(),
    .has_pending = WAIT_QUEUE_INIT(klog.has_pending),
    .buf = {
        .buffer = klog_buffer,
        .len = sizeof(klog_buffer),
    },
};

static void klog_print(struct kp_output *out, const char *str)
{
    using_spinlock(&klog.lock) {
        char_buf_write(&klog.buf, str, strlen(str));
        wait_queue_wake(&klog.has_pending);
    }
}

static struct kp_output_ops klog_kp_output_ops = {
    .print = klog_print,
};

static struct kp_output klog_kp_output = KP_OUTPUT_INIT(klog_kp_output, KP_TRACE, "klog", &klog_kp_output_ops);

void klog_init(void)
{
    kp_output_register(&klog_kp_output);
}

static int klog_read(struct file *filp, struct user_buffer buf, size_t size)
{
    int ret = 0;
    size_t have_read = 0;

    struct page *page = palloc(0, PAL_KERNEL);
    if (!page)
        return -ENOMEM;

    while (size) {
        size_t to_read = size > PG_SIZE? PG_SIZE: size;
        size_t bytes_read = 0;

        /* We can't write to userspace while holding the spinlock */
        using_spinlock(&klog.lock)
            bytes_read = char_buf_read(&klog.buf, page->virt, to_read);

        ret = user_memcpy_from_kernel(user_buffer_index(buf, have_read), page->virt, bytes_read);
        if (ret)
            goto pfree_ret;

        have_read += bytes_read;
        size -= bytes_read;

        /* If we got back less bytes then we asked from the char_buf, then it
         * is now empty and we exit early */
        if (to_read != bytes_read)
            break;
    }

  pfree_ret:
    pfree(page, 0);
    if (ret < 0)
        return ret;
    else
        return have_read;
}

static int klog_poll(struct file *filp, struct poll_table *table, int events)
{
    int ret = 0;

    if (flag_test(&filp->flags, FILE_READABLE) && events & POLLIN)
        poll_table_add(table, &klog.has_pending);

    using_spinlock(&klog.lock) {
        if (flag_test(&filp->flags, FILE_READABLE) && events & POLLIN) {
            if (char_buf_has_data(&klog.buf))
                ret |= POLLIN;
        }
    }

    return ret;
}

const struct file_ops klog_file_ops = {
    .read = klog_read,
    .poll = klog_poll,
};
