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
#include <protura/dev.h>
#include <protura/mm/palloc.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/user_check.h>
#include <protura/snprintf.h>

#include <arch/spinlock.h>
#include <arch/asm.h>
#include <protura/fs/char.h>
#include <protura/fs/fcntl.h>
#include <protura/event/device.h>
#include <protura/drivers/screen.h>
#include <protura/drivers/keyboard.h>
#include <protura/drivers/console.h>
#include <protura/drivers/com.h>
#include <protura/drivers/tty.h>
#include "tty_termios.h"

const struct termios default_termios = {
    .c_iflag = ICRNL,
    .c_oflag = OPOST | ONLCR,
    .c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOCTL,
    .c_cflag = B38400,
    .c_cc = {
        [VINTR] = 0x03,
        [VERASE] = 0x7F,
        [VSUSP] = 0x1A,
        [VEOF] = 0x04,
    },
};

static const struct winsize default_winsize = {
	.ws_row = 25,
	.ws_col = 80,
	.ws_xpixel = 0,
	.ws_ypixel = 0,
};

/*
 * The TTY system sits in-between the terminal (The combination of the
 * screen+keyboard), and the processes being run (Ex. Shell). The kernel does
 * processing on the characters received from the terminal and sends them to the
 * process, and then also does processing on the characters received from the
 * process, and eventually sends them to the terminal.
 *
 * The settings for the TTY are controlled via the `termios` structure (Which
 * userspace can modify), and most of tha handling happens in tty_termios.c
 *
 * When the hardware has data ready, it saves it into an internal buffer
 * (somewhere) and wake's up the tty's work entry. The tty_pump will then be
 * scheduled onto any existing kwork threads to have the data from the hardware
 * processed.
 *
 * Processes using the tty call the tty_read function from a `struct file`. The
 * `tty_read` function then attempts to read from the buffer on the tty holding
 * data ready to be read. If not enough data is there, the process sleeps on
 * the `in_wait_queue`. The kernel-thread wakes up the `in_wait_queue` after
 * processing data that results in data ready to be sent to the process.
 *
 * For data coming in via processes, they call tty_write, at which point the
 * data is immediately processed by the kernel as it comes in.
 */

static spinlock_t tty_list_lock = SPINLOCK_INIT();
static list_head_t tty_list = LIST_HEAD_INIT(tty_list);

void tty_pump(struct work *work)
{
    struct tty *tty = container_of(work, struct tty, work);
    char buf[64];
    size_t buf_len = 0;

    using_spinlock(&tty->input_buf_lock)
        buf_len = char_buf_read(&tty->input_buf, buf, sizeof(buf));

    /* If it's possible input_buf still has data, then schedule us to run again
     * to read more */
    if (buf_len == sizeof(buf))
        work_schedule(work);

    tty_process_input(tty, buf, buf_len);
}

static void tty_create(struct tty_driver *driver, dev_t devno)
{
    struct tty *tty = kmalloc(sizeof(*tty), PAL_KERNEL);

    if (!tty)
        return ;

    tty_init(tty);

    tty->driver = driver;
    tty->device_no = devno;

    tty->output_buf.buffer = palloc_va(0, PAL_KERNEL);
    tty->output_buf.len = PG_SIZE;

    tty->input_buf.buffer = palloc_va(0, PAL_KERNEL);
    tty->input_buf.len = PG_SIZE;

    tty->winsize = default_winsize;
    tty->termios = default_termios;
    kp(KP_TRACE, "Termios setting: %d\n", tty->termios.c_lflag);

    tty->line_buf = palloc_va(0, PAL_KERNEL);
    tty->line_buf_size = PG_SIZE;
    tty->line_buf_pos = 0;

    using_spinlock(&tty_list_lock)
        list_add_tail(&tty_list, &tty->tty_node);

    (driver->ops->init) (tty);

    device_submit_char(KERN_EVENT_DEVICE_ADD, devno);
}

void tty_driver_register(struct tty_driver *driver)
{
    size_t i;
    for (i = driver->minor_start; i <= driver->minor_end; i++)
        tty_create(driver, DEV_MAKE(driver->major, i));
}

static struct tty *tty_find(dev_t dev)
{
    struct tty *tty;

    using_spinlock(&tty_list_lock) {
        list_foreach_entry(&tty_list, tty, tty_node) {
            if (tty->device_no != dev)
                continue;

            return tty;
        }
    }

    return NULL;
}

void tty_add_input(struct tty *tty, const char *buf, size_t len)
{
    using_spinlock(&tty->input_buf_lock) {
        char_buf_write(&tty->input_buf, buf, len);
        work_schedule(&tty->work);
    }
}

void tty_add_input_str(struct tty *tty, const char *str)
{
    size_t len = strlen(str);

    using_spinlock(&tty->input_buf_lock) {
        char_buf_write(&tty->input_buf, str, len);
        work_schedule(&tty->work);
    }
}

void tty_flush_input(struct tty *tty)
{
    using_spinlock(&tty->input_buf_lock)
        char_buf_clear(&tty->input_buf);
}

void tty_flush_output(struct tty *tty)
{
    using_mutex(&tty->lock)
        char_buf_clear(&tty->output_buf);
}

static int tty_read(struct file *filp, struct user_buffer vbuf, size_t len)
{
    struct tty *tty = filp->priv_data;
    size_t orig_len = len;
    int ret = 0;

    if (!tty)
        return -ENOTTY;

    using_mutex(&tty->lock) {
        while (orig_len == len) {
            size_t read_count;

            read_count = char_buf_read_user(&tty->output_buf, vbuf, len);

            vbuf = user_buffer_index(vbuf, read_count);
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
            ret = wait_queue_event_intr_mutex(&tty->in_wait_queue, char_buf_has_data(&tty->output_buf) || tty->ret0, &tty->lock);
            if (ret)
                return ret;
        }
    }

    if (!ret)
        return orig_len - len;
    else
        return ret;
}

/* Used for things like the seg-fault message */
int tty_write_buf_user(struct tty *tty, struct user_buffer buf, size_t len)
{
    if (!tty)
        return -ENOTTY;

    return tty_process_output(tty, buf, len);
}

int tty_write_buf(struct tty *tty, const char *buf, size_t len)
{
    return tty_write_buf_user(tty, make_kernel_buffer(buf), len);
}

static int tty_write(struct file *filp, struct user_buffer vbuf, size_t len)
{
    return tty_write_buf_user(filp->priv_data, vbuf, len);
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
    int major = DEV_MAJOR(inode->dev_no);
    dev_t minor = DEV_MINOR(inode->dev_no);
    struct tty *tty;

    /* Special case for /dev/tty - open the current controlling TTY */
    if (minor == 0 && major == CHAR_DEV_TTY) {
        if (!current->tty)
            return -ENXIO;

        filp->priv_data = current->tty;
        return 0;
    }

    tty = tty_find(inode->dev_no);
    if (!tty)
        return -ENXIO;

    filp->priv_data = tty;

    kp(KP_TRACE, "tty_open: noctty: %d, slead: %d, cur->tty: %p, id: %d\n",
            noctty, flag_test(&current->flags, TASK_FLAG_SESSION_LEADER), current->tty, tty->session_id);

    if (!noctty && flag_test(&current->flags, TASK_FLAG_SESSION_LEADER) && !current->tty && tty->session_id == 0) {
        current->tty = tty;
        using_mutex(&tty->lock) {
            tty->session_id = current->session_id;
            tty->fg_pgrp = current->pgid;
            char_buf_clear(&tty->output_buf);
        }
    }

    return 0;
}

static int tty_ioctl(struct file *filp, int cmd, struct user_buffer arg)
{
    int ret;
    int state;
    int console;
    struct task *current = cpu_get_local()->current;
    struct tty *tty = filp->priv_data;
    pid_t tmp;
    struct termios tmp_tios;
    struct winsize tmp_wins;

    kp(KP_TRACE, "tty_ioctl: tty: %p, cmd: %d, ctty: %p\n", tty, cmd, current->tty);

    if ((cmd >> 8) != __TIO && tty != current->tty)
        return -ENOTTY;

    switch (cmd) {
    case TIOCGPGRP:
        kp(KP_TRACE, "tty_ioctl: gpgrp\n");

        using_mutex(&tty->lock)
            tmp = tty->fg_pgrp;

        return user_copy_from_kernel(arg, tmp);

    case TIOCSPGRP:
        kp(KP_TRACE, "tty_ioctl: spgrp\n");
        ret = user_copy_to_kernel(&tmp, arg);
        if (ret)
            return ret;

        using_mutex(&tty->lock)
            tty->fg_pgrp = tmp;

        return 0;

    case TIOCGSID:
        kp(KP_TRACE, "tty_ioctl: gsid\n");

        using_mutex(&tty->lock)
            tmp  = tty->session_id;

        return user_copy_from_kernel(arg, tmp);

    case TCGETS:
        kp(KP_TRACE, "tty_ioctl: get termios\n");

        using_mutex(&tty->lock)
            tmp_tios = tty->termios;

        return user_copy_from_kernel(arg, tmp_tios);

    case TCSETS:
        kp(KP_TRACE, "tty_ioctl: set termios\n");
        ret = user_copy_to_kernel(&tmp_tios, arg);
        if (ret)
            return ret;

        using_mutex(&tty->lock)
            tty->termios = tmp_tios;

        tty_line_buf_drain(tty);
        return 0;

    case TIOCGWINSZ:
        kp(KP_TRACE, "tty_ioctl: get winsize\n");

        using_mutex(&tty->lock)
            tmp_wins = tty->winsize;

        return user_copy_from_kernel(arg, tmp_wins);

    case TIOCSWINSZ:
        kp(KP_TRACE, "tty_ioctl: set winsize\n");

        ret = user_copy_to_kernel(&tmp_wins, arg);
        if (ret)
            return ret;

        using_mutex(&tty->lock)
            tty->winsize = tmp_wins;

        return 0;

    case TCFLSH:
        switch ((uintptr_t)arg.ptr) {
        case TCIFLUSH:
            tty_flush_input(tty);
            break;

        case TCOFLUSH:
            tty_flush_output(tty);
            break;

        case TCIOFLUSH:
            tty_flush_input(tty);
            tty_flush_output(tty);
            break;
        }
        break;

    case TIOSETKBD:
        state = (int)arg.ptr;
        if (state != TTY_KEYBOARD_STATE_ON && state != TTY_KEYBOARD_STATE_OFF)
            return -EINVAL;

        keyboard_set_state(state);
        return 0;

    case TIOSETCONSOLE:
        console = (int)arg.ptr;

        if (console < 0 || console >= CONSOLE_MAX)
            return -EINVAL;

        console_switch_vt(console);
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
    device_submit_char(KERN_EVENT_DEVICE_ADD, DEV_MAKE(CHAR_DEV_TTY, 0));

#ifdef CONFIG_CONSOLE_DRIVER
    vt_console_init();
#endif
    com_tty_init();
}

