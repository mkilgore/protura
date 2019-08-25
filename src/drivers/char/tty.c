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
#include <protura/wait.h>
#include <protura/mm/palloc.h>
#include <protura/mm/kmalloc.h>
#include <protura/snprintf.h>

#include <arch/spinlock.h>
#include <protura/drivers/term.h>
#include <arch/asm.h>
#include <protura/fs/char.h>
#include <protura/fs/fcntl.h>
#include <protura/drivers/screen.h>
#include <protura/drivers/keyboard.h>
#include <protura/drivers/console.h>
#include <protura/drivers/com.h>
#include <protura/drivers/tty.h>
#include "tty_termios.h"

const struct termios default_termios = {
    .c_iflag = ICRNL,
    .c_oflag = OPOST | ONLCR,
    .c_lflag = ISIG | ICANON | ECHO | ECHOE,
    .c_cflag = B38400,
    .c_cc = {
        [VINTR] = 0x03,
        [VERASE] = 0x7F,
        [VSUSP] = 0x1A,
    },
};

static const struct winsize default_winsize = {
	.ws_row = 25,
	.ws_col = 80,
	.ws_xpixel = 0,
	.ws_ypixel = 0,
};

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

void tty_pump(struct work *work)
{
    struct tty *tty = container_of(work, struct tty, work);
    const struct tty_driver *driver = tty->driver;

    uint32_t start_ticks = timer_get_ms();

    kp(KP_NORMAL, "TTY pump start\n");

    while (driver->ops->has_chars(tty) || char_buf_has_data(&tty->input_buf)) {
        tty_process_input(tty);
        tty_process_output(tty);

        /* If we've been pumping for 2MS, then we reschedule ourselves and exit, so we don't hold-up the `kwork` queue. */
        if (timer_get_ms() >= start_ticks + 2) {
            kp(KP_NORMAL, "TTY pump timer trigger\n");
            work_schedule(work);
            break;
        }
    }

    kp(KP_NORMAL, "TTY pump end\n");
}

static void tty_create(struct tty_driver *driver, dev_t devno)
{
    struct tty *tty = kmalloc(sizeof(*tty), PAL_KERNEL);

    if (!tty)
        return ;

    tty_init(tty);

    tty->driver = driver;
    tty->device_no = devno;

    kp(KP_TRACE, "TTY number name\n");
    int ttyno = devno;
    int digits = (ttyno < 10)? 1:
                 (ttyno < 100)? 2: 3;
    size_t name_len = strlen(driver->name) + digits;
    kp(KP_TRACE, "TTY digits: %d, devno: %d, name_len: %d\n", digits, ttyno, name_len);

    tty->name = kmalloc(name_len + 1, PAL_KERNEL);
    snprintf(tty->name, name_len + 1, "%s%d", driver->name, ttyno);

    kp(KP_TRACE, "TTY %s%d name: %s\n", driver->name, ttyno, tty->name);

    tty->input_buf.buffer = palloc_va(0, PAL_KERNEL);
    tty->input_buf.len = PG_SIZE;

    tty->output_buf.buffer = palloc_va(0, PAL_KERNEL);
    tty->output_buf.len = PG_SIZE;

    tty->winsize = default_winsize;
    tty->termios = default_termios;
    kp(KP_TRACE, "Termios setting: %d\n", tty->termios.c_lflag);

    tty->line_buf = palloc_va(0, PAL_KERNEL);
    tty->line_buf_size = PG_SIZE;
    tty->line_buf_pos = 0;

    tty_array[devno + driver->minor_start] = tty;

    driver->ops->register_for_wakeups(tty);
}

void tty_driver_register(struct tty_driver *driver)
{
    dev_t devices = driver->minor_end - driver->minor_start + 1;
    size_t i;

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
    struct tty *tty = filp->priv_data;
    size_t orig_len = len;
    int ret = 0;

    if (!tty)
        return -ENOTTY;

    using_mutex(&tty->lock) {
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

            if (flag_test(&filp->flags, FILE_NONBLOCK)) {
                ret = -EAGAIN;
                break;
            }

            /* Nice little dance to wait for data or a signal */
            kp(KP_NORMAL, "tty_read: tty->output_buf wait\n");
            ret = wait_queue_event_intr_mutex(&tty->in_wait_queue, char_buf_has_data(&tty->output_buf) || tty->ret0, &tty->lock);
            kp(KP_NORMAL, "tty_read: tty->output_buf end wait: %d\n", ret);
            if (ret) {
                kp(KP_NORMAL, "Exiting...\n");
                return ret;
            }
        }
    }

    if (!ret)
        return orig_len - len;
    else
        return ret;
}

/* Used for things like the seg-fault message */
int tty_write_buf(struct tty *tty, const char *buf, size_t len)
{
    if (!tty)
        return -ENOTTY;

    using_mutex(&tty->lock) {
        char_buf_write(&tty->input_buf, buf, len);
        work_schedule(&tty->work);
    }

    return 0;
}

static int tty_write(struct file *filp, const void *vbuf, size_t len)
{
    struct tty *tty = filp->priv_data;
    size_t orig_len = len;

    int ret = tty_write_buf(tty, vbuf, len);
    if (ret)
        return ret;

    return orig_len;
}

static int tty_poll(struct file *filp, struct poll_table *table, int events)
{
    struct tty *tty = filp->priv_data;
    int ret = 0;

    if (!tty)
        return POLLERR;

    using_mutex(&tty->lock) {
        if (events & POLLIN) {
            if (char_buf_has_data(&tty->output_buf))
                ret |= POLLIN;

            poll_table_add(table, &tty->in_wait_queue);
        }

        if (events & POLLOUT)
            ret |= POLLOUT;
    }

    return ret;
}

static int tty_open(struct inode *inode, struct file *filp)
{
    struct task *current = cpu_get_local()->current;
    int noctty = flag_test(&filp->flags, FILE_NOCTTY);
    dev_t minor = DEV_MINOR(inode->dev_no);
    struct tty *tty;

    if (minor == 0) {
        if (!current->tty)
            return -ENXIO;

        filp->priv_data = current->tty;
        return 0;
    }

    tty = tty_find(minor);
    filp->priv_data = tty;

    kp(KP_TRACE, "tty_open: noctty: %d, slead: %d, cur->tty: %p, id: %d\n",
            noctty, flag_test(&current->flags, TASK_FLAG_SESSION_LEADER), current->tty, tty->session_id);

    if (!noctty && flag_test(&current->flags, TASK_FLAG_SESSION_LEADER) && !current->tty && tty->session_id == 0) {
        current->tty = tty;
        tty->session_id = current->session_id;
        tty->fg_pgrp = current->pgid;
    }

    return 0;
}

static int tty_ioctl(struct file *filp, int cmd, uintptr_t arg)
{
    pid_t *parg;
    struct termios *tios;
    struct winsize *wins;
    int ret;
    struct task *current = cpu_get_local()->current;
    struct tty *tty = filp->priv_data;

    kp(KP_TRACE, "tty_ioctl: tty: %p, cmd: %d, ctty: %p\n", tty, cmd, current->tty);

    if ((cmd >> 8) != __TIO && tty != current->tty)
        return -ENOTTY;

    switch (cmd) {
    case TIOCGPGRP:
        kp(KP_TRACE, "tty_ioctl: gpgrp\n");
        parg = (pid_t *)arg;
        ret = user_check_region(parg, sizeof(*parg), F(VM_MAP_WRITE));
        if (ret)
            return ret;

        using_mutex(&tty->lock)
            *parg = tty->fg_pgrp;
        return 0;

    case TIOCSPGRP:
        kp(KP_TRACE, "tty_ioctl: spgrp\n");
        parg = (pid_t *)arg;
        ret = user_check_region(parg, sizeof(*parg), F(VM_MAP_READ));
        if (ret)
            return ret;

        using_mutex(&tty->lock)
            tty->fg_pgrp = *parg;
        return 0;

    case TIOCGSID:
        kp(KP_TRACE, "tty_ioctl: gsid\n");
        parg = (pid_t *)arg;
        ret = user_check_region(parg, sizeof(*parg), F(VM_MAP_WRITE));
        if (ret)
            return ret;

        using_mutex(&tty->lock)
            *parg = tty->session_id;
        return 0;

    case TCGETS:
        kp(KP_TRACE, "tty_ioctl: get termios\n");
        tios = (struct termios *)arg;
        ret = user_check_region(tios, sizeof(*tios), F(VM_MAP_WRITE));
        if (ret)
            return ret;

        using_mutex(&tty->lock)
            *tios = tty->termios;
        return 0;

    case TCSETS:
        kp(KP_TRACE, "tty_ioctl: set termios\n");
        tios = (struct termios *)arg;
        ret = user_check_region(tios, sizeof(*tios), F(VM_MAP_READ));
        if (ret)
            return ret;

        using_mutex(&tty->lock)
            tty->termios = *tios;

        tty_line_buf_flush(tty);
        return 0;

    case TIOCGWINSZ:
        kp(KP_TRACE, "tty_ioctl: get winsize\n");
        wins = (struct winsize *)arg;
        ret = user_check_region(wins, sizeof(*wins), F(VM_MAP_WRITE));
        if (ret)
            return ret;

        using_mutex(&tty->lock)
            *wins = tty->winsize;
        return 0;

    case TIOCSWINSZ:
        kp(KP_TRACE, "tty_ioctl: set winsize\n");
        wins = (struct winsize *)arg;
        ret = user_check_region(wins, sizeof(*wins), F(VM_MAP_READ));
        if (ret)
            return ret;

        using_mutex(&tty->lock)
            tty->winsize = *wins;
        return 0;
    }

    kp(KP_TRACE, "tty_ioctl: INVALID CMD: 0x%04x\n", cmd);
    return -EINVAL;
}

struct file_ops tty_file_ops = {
    .open = tty_open,
    .read = tty_read,
    .write = tty_write,
    .poll = tty_poll,
    .ioctl = tty_ioctl,
};

void tty_subsystem_init(void)
{
#ifdef CONFIG_CONSOLE_DRIVER
    console_init();
#endif
    com_tty_init();
}

