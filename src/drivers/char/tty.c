/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/scheduler.h>
#include <protura/kassert.h>
#include <protura/wait.h>
#include <protura/mm/palloc.h>

#include <arch/spinlock.h>
#include <protura/drivers/term.h>
#include <arch/asm.h>
#include <protura/fs/char.h>
#include <protura/drivers/screen.h>
#include <protura/drivers/keyboard.h>
#include <protura/drivers/console.h>
#include <protura/drivers/tty.h>

/*
 * Complicated and messy
 *
 * Works this way:
 *
 * Each tty has a coresponding kernel-thread which facalitates the transfer of
 * data from a user-space program to the underlying hardware, and performs tty
 * functions as necessary.
 *
 * When the hardware has data ready, it saves it into an internal buffer
 * (somewhere) and wake's up the coresponding tty's kernel thread. The kernel
 * thread then goes and attemps to read all the data it can until that fails,
 * and then goes back to sleep after processing the data.
 *
 * Processes using the tty call the tty_read function from a `struct file`. The
 * `tty_read` function then attempts to read from the buffer on the tty holding
 * data ready to be read. If not enough data is there, the process sleeps on
 * the `in_wait_queue`. The kernel-thread wakes up the `in_wait_queue` after
 * processing data that results in data ready to be sent to the process.
 *
 * Processes writing to the tty write the data into the tty's buffer and
 * wake-up the kernel-thread. The kernel thread checks if there is any data to
 * be read from the process, and then processes it when so.
 */
static list_head_t tty_driver_list = LIST_HEAD_INIT(tty_driver_list);

/* Currently does a simple echo from the tty input to the tty output */
static int tty_kernel_thread(void *p)
{
    struct tty_driver *driver = p;
    char line_buf[128];
    size_t start = 0;

    while (1) {
        char buf[32];
        size_t buf_len;
        int i;

        buf_len = (driver->ops->read) (driver, buf, sizeof(buf));

        for (i = 0; i < buf_len; i++) {
            switch (buf[i]) {
            case '\n':
                driver->ops->write(driver, buf + i, 1);
                line_buf[start++] = '\n';
                line_buf[start] = '\0';
                using_mutex(&driver->inout_buf_lock) {
                    char_buf_write(&driver->output_buf, line_buf, start);
                    wait_queue_wake(&driver->in_wait_queue);
                }
                start = 0;
                break;

            case '\b':
                if (start > 0) {
                    start--;
                    driver->ops->write(driver, buf + i, 1);
                }
                break;

            case '\x03':
                driver->ops->write(driver, "^C", 2);
                break;

            case '\x04':
                driver->ret0 = 1;
                wait_queue_wake(&driver->in_wait_queue);
                break;

            default:
                driver->ops->write(driver, buf + i, 1);
                line_buf[start++] = buf[i];
                break;
            }
        }

        /* For now, the process output is written directly to the hardware driver */
        buf_len = char_buf_read(&driver->input_buf, buf, sizeof(buf));

        if (buf_len)
            driver->ops->write(driver, buf, buf_len);

        sleep {
            if (!driver->ops->has_chars(driver) && !char_buf_has_data(&driver->input_buf))
                scheduler_task_yield();
        }
    }

    return 0;
}

void tty_register(struct tty_driver *driver)
{
    driver->input_buf.buffer = palloc_va(0, PAL_KERNEL);
    driver->input_buf.len = PG_SIZE;

    driver->output_buf.buffer = palloc_va(0, PAL_KERNEL);
    driver->output_buf.len = PG_SIZE;

    driver->kernel_task = task_kernel_new_interruptable(driver->name, tty_kernel_thread, driver);
    scheduler_task_add(driver->kernel_task);

    list_add(&tty_driver_list, &driver->tty_driver_node);
}

static struct tty_driver *tty_driver_find(dev_t minor)
{
    struct tty_driver *d;
    list_foreach_entry(&tty_driver_list, d, tty_driver_node)
        if (d->minor_start <= minor && d->minor_end >= minor)
            return d;

    return NULL;
}

static int tty_read(struct file *filp, void *vbuf, size_t len)
{
    struct inode *i = filp->inode;
    dev_t minor = DEV_MINOR(i->dev_no);
    struct tty_driver *driver = tty_driver_find(minor);
    size_t orig_len = len;

    if (!driver)
        return -ENOTTY;

    using_mutex(&driver->inout_buf_lock) {
        while (orig_len == len) {
            size_t read_count;

            read_count = char_buf_read(&driver->output_buf, vbuf, len);

            vbuf += read_count;
            len -= read_count;

            /* The ret0 flag forces an immediate return from read.
             * When there is no data, we end-up returning zero, which
             * represents EOF */
            if (driver->ret0) {
                driver->ret0 = 0;
                break;
            }

            if (len != orig_len)
                break;

            /* Nice little dance to wait for data or a singal */
            sleep_intr_with_wait_queue(&driver->in_wait_queue) {
                if (!char_buf_has_data(&driver->output_buf)) {
                    not_using_mutex(&driver->inout_buf_lock)  {
                        scheduler_task_yield();

                        if (has_pending_signal(cpu_get_local()->current)) {
                            wait_queue_unregister();
                            return -ERESTARTSYS;
                        }
                    }
                }
            }
        }
    }

    return orig_len - len;
}

static int tty_write(struct file *filp, const void *vbuf, size_t len)
{
    struct inode *i = filp->inode;
    dev_t minor = DEV_MINOR(i->dev_no);
    struct tty_driver *driver = tty_driver_find(minor);
    size_t orig_len = len;

    if (!driver)
        return -ENOTTY;

    using_mutex(&driver->inout_buf_lock) {
        char_buf_write(&driver->input_buf, vbuf, len);
        scheduler_task_wake(driver->kernel_task);
    }

    return orig_len;
}

struct file_ops tty_file_ops = {
    .read = tty_read,
    .write = tty_write,
};

void tty_init(void)
{
    console_init();
}

