/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_TTY_H
#define INCLUDE_PROTURA_TTY_H

#include <uapi/protura/drivers/tty.h>
#include <protura/list.h>
#include <protura/char_buf.h>
#include <protura/spinlock.h>
#include <protura/mutex.h>
#include <protura/work.h>
#include <protura/wait.h>

extern struct file_ops tty_file_ops;

struct tty_driver;
struct tty;
struct file;

struct tty_ops {
    void (*init) (struct tty *);
    int (*write) (struct tty *, const char *, size_t size);
};

struct tty_driver {
    int major;
    int minor_start;
    int minor_end;
    const struct tty_ops *ops;
};

struct tty {
    dev_t device_no;

    list_node_t tty_node;

    mutex_t lock;

    struct work work;

    pid_t session_id;
    pid_t fg_pgrp;

    struct winsize winsize;
    struct termios termios;
    struct tty_driver *driver;

    /*
     * Keyboard input is put into the `input_buf`
     */
    spinlock_t input_buf_lock;
    struct char_buf input_buf;

    /* 
     * Processes read from the 'output_buf'
     */
    struct wait_queue in_wait_queue;

    struct char_buf output_buf;
    int ret0; /* ^D marker */

    char *line_buf;
    size_t line_buf_pos;
    size_t line_buf_size;
};

void tty_pump(struct work *);

#define TTY_INIT(tty) \
    { \
        .tty_node = LIST_NODE_INIT((tty).tty_node), \
        .input_buf_lock = SPINLOCK_INIT(), \
        .lock = MUTEX_INIT((tty).lock), \
        .work = WORK_INIT_KWORK((tty).work, tty_pump), \
        .in_wait_queue = WAIT_QUEUE_INIT((tty).in_wait_queue), \
    }

static inline void tty_init(struct tty *tty)
{
    *tty = (struct tty)TTY_INIT(*tty);
}

void tty_driver_register(struct tty_driver *driver);
int tty_write_buf(struct tty *tty, const char *buf, size_t len);
int tty_write_buf_user(struct tty *tty, struct user_buffer buf, size_t len);

int tty_resize(struct tty *tty, const struct winsize *new_size);

/* This allows add data to the *input* side of the tty, as though it was typed
 * on the keyboard.
 *
 * Note: The buffer may be modified */
void tty_add_input(struct tty *, const char *buf, size_t len);
void tty_add_input_str(struct tty *, const char *str);
void tty_flush_input(struct tty *tty);
void tty_flush_output(struct tty *tty);

#define __TERMIOS_FLAG_OFLAG(termios, flag) (((termios)->c_oflag & (flag)) != 0)
#define __TERMIOS_FLAG_CFLAG(termios, flag) (((termios)->c_cflag & (flag)) != 0)
#define __TERMIOS_FLAG_LFLAG(termios, flag) (((termios)->c_lflag & (flag)) != 0)
#define __TERMIOS_FLAG_IFLAG(termios, flag) (((termios)->c_iflag & (flag)) != 0)

#define TERMIOS_OPOST(termios) __TERMIOS_FLAG_OFLAG((termios), OPOST)
#define TERMIOS_ISIG(termios) __TERMIOS_FLAG_LFLAG((termios), ISIG)
#define TERMIOS_ICANON(termios) __TERMIOS_FLAG_LFLAG((termios), ICANON)
#define TERMIOS_ECHO(termios) __TERMIOS_FLAG_LFLAG((termios), ECHO)
#define TERMIOS_ECHOE(termios) __TERMIOS_FLAG_LFLAG((termios), ECHOE)
#define TERMIOS_ECHOCTL(termios) __TERMIOS_FLAG_LFLAG((termios), ECHOCTL)
#define TERMIOS_NOFLSH(termios) __TERMIOS_FLAG_LFLAG((termios), NOFLSH)

#define TERMIOS_IGNBRK(termios) __TERMIOS_FLAG_IFLAG((termios), IGNBRK)
#define TERMIOS_ISTRIP(termios) __TERMIOS_FLAG_IFLAG((termios), ISTRIP)
#define TERMIOS_INLCR(termios)  __TERMIOS_FLAG_IFLAG((termios), INLCR)
#define TERMIOS_IGNCR(termios)  __TERMIOS_FLAG_IFLAG((termios), IGNCR)
#define TERMIOS_ICRNL(termios)  __TERMIOS_FLAG_IFLAG((termios), ICRNL)
#define TERMIOS_IUCLC(termios)  __TERMIOS_FLAG_IFLAG((termios), IUCLC)

#define TERMIOS_OLCUC(termios)  __TERMIOS_FLAG_OFLAG((termios), OLCUC)
#define TERMIOS_ONLCR(termios)  __TERMIOS_FLAG_OFLAG((termios), ONLCR)
#define TERMIOS_OCRNL(termios)  __TERMIOS_FLAG_OFLAG((termios), OCRNL)
#define TERMIOS_ONLRET(termios) __TERMIOS_FLAG_OFLAG((termios), ONLRET)

#endif
