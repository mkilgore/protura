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
#include <protura/mm/kmalloc.h>

#include <arch/spinlock.h>
#include <protura/drivers/term.h>
#include <arch/asm.h>
#include <protura/fs/char.h>
#include <protura/drivers/screen.h>
#include <protura/drivers/keyboard.h>
#include <protura/drivers/console.h>
#include <protura/drivers/com.h>
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

#define MAX_MINOR 32

static struct tty *tty_array[MAX_MINOR];

/* Currently does a simple echo from the tty input to the tty output */
static int tty_kernel_thread(void *p)
{
    struct tty *tty = p;
    struct tty_driver *driver = tty->driver;
    char line_buf[128];
    size_t start = 0;

    (driver->ops->register_for_wakeups) (tty);

    while (1) {
        char buf[32];
        size_t buf_len;
        int i;

        buf_len = (driver->ops->read) (tty, buf, sizeof(buf));

        for (i = 0; i < buf_len; i++) {
            switch (buf[i]) {
            case '\n':
                driver->ops->write(tty, buf + i, 1);
                line_buf[start++] = '\n';
                line_buf[start] = '\0';
                using_mutex(&tty->inout_buf_lock) {
                    char_buf_write(&tty->output_buf, line_buf, start);
                    wait_queue_wake(&tty->in_wait_queue);
                }
                start = 0;
                break;

            case '\b':
            case '\x7f':
                if (start > 0) {
                    start--;
                    driver->ops->write(tty, &(char){ '\b' }, 1);
                    driver->ops->write(tty, &(char){ ' ' }, 1);
                    driver->ops->write(tty, &(char){ '\b' }, 1);
                }
                break;

            case '\x04':
                tty->ret0 = 1;
                wait_queue_wake(&tty->in_wait_queue);
                break;

            default:
                driver->ops->write(tty, buf + i, 1);
                line_buf[start++] = buf[i];
                break;
            }
        }

        /* For now, the process output is written directly to the hardware driver */
        using_mutex(&tty->inout_buf_lock)
            buf_len = char_buf_read(&tty->input_buf, buf, sizeof(buf));

        if (buf_len) {
            driver->ops->write(tty, buf, buf_len);
        }

        sleep {
            if (!driver->ops->has_chars(tty) && !char_buf_has_data(&tty->input_buf))
                scheduler_task_yield();
        }
    }

    return 0;
}

static void tty_create(struct tty_driver *driver, dev_t devno)
{
    struct tty *tty = kmalloc(sizeof(*tty), PAL_KERNEL);

    if (!tty)
        return ;

    tty_init(tty);

    tty->driver = driver;
    tty->device_no = devno;

    tty->input_buf.buffer = palloc_va(0, PAL_KERNEL);
    tty->input_buf.len = PG_SIZE;

    tty->output_buf.buffer = palloc_va(0, PAL_KERNEL);
    tty->output_buf.len = PG_SIZE;

    tty->kernel_task = task_kernel_new_interruptable(driver->name, tty_kernel_thread, tty);
    scheduler_task_add(tty->kernel_task);

    tty_array[devno + driver->minor_start] = tty;
}

void tty_driver_register(struct tty_driver *driver)
{
    dev_t devices = driver->minor_end - driver->minor_start + 1;
    int i;

    for (i = 0; i < devices; i++) {
        kp(KP_TRACE, "Creating %s%d\n", driver->name, i);
        tty_create(driver, i);
    }

    return ;
}

static struct tty *tty_find(dev_t minor)
{
    if (minor >= MAX_MINOR)
        return NULL;

    return tty_array[minor];
}

static int tty_read(struct file *filp, void *vbuf, size_t len)
{
    struct inode *i = filp->inode;
    dev_t minor = DEV_MINOR(i->dev_no);
    struct tty *tty = tty_find(minor);
    size_t orig_len = len;

    if (!tty)
        return -ENOTTY;

    using_mutex(&tty->inout_buf_lock) {
        while (orig_len == len) {
            size_t read_count;

            read_count = char_buf_read(&tty->output_buf, vbuf, len);

            vbuf += read_count;
            len -= read_count;

            /* The ret0 flag forces an immediate return from read.
             * When there is no data, we end-up returning zero, which
             * represents EOF */
            if (tty->ret0) {
                tty->ret0 = 0;
                break;
            }

            if (len != orig_len)
                break;

            /* Nice little dance to wait for data or a singal */
            sleep_intr_with_wait_queue(&tty->in_wait_queue) {
                if (!char_buf_has_data(&tty->output_buf)) {
                    not_using_mutex(&tty->inout_buf_lock)  {
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
    struct tty *tty = tty_find(minor);
    size_t orig_len = len;

    if (!tty)
        return -ENOTTY;

    using_mutex(&tty->inout_buf_lock) {
        char_buf_write(&tty->input_buf, vbuf, len);
        scheduler_task_wake(tty->kernel_task);
    }

    return orig_len;
}

struct file_ops tty_file_ops = {
    .read = tty_read,
    .write = tty_write,
};

void tty_subsystem_init(void)
{
    console_init();
    com_tty_init();
}

