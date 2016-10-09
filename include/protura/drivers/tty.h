/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_TTY_H
#define INCLUDE_PROTURA_TTY_H

#if __KERNEL__
# include <protura/types.h>
# include <protura/list.h>
# include <protura/char_buf.h>
# include <protura/spinlock.h>
# include <protura/mutex.h>
#endif

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

#define NCCS 19
struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_line;
    cc_t c_cc[NCCS];
};

/* c_cc characters */
#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6
#define VSWTC 7
#define VSTART 8
#define VSTOP 9
#define VSUSP 10
#define VEOL 11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE 14
#define VLNEXT 15
#define VEOL2 16

#define __TIO 90

#define TIOCGPGRP ((__TIO << 8) + 0)
#define TIOCSPGRP ((__TIO << 8) + 1)
#define TIOCGSID  ((__TIO << 8) + 2)

#if __KERNEL__

extern struct file_ops tty_file_ops;
void tty_subsystem_init(void);

struct tty_driver;
struct tty;
struct file;

struct tty_ops {
    void (*register_for_wakeups) (struct tty *);
    int (*has_chars) (struct tty *);
    int (*read) (struct tty *, char *, size_t);
    int (*write) (struct tty *, const char *, size_t size);
};

struct tty_driver {
    const char *name;
    dev_t minor_start;
    dev_t minor_end;
    struct tty_ops *ops;
};

struct tty {
    char *name;
    /* Device number for the driver Minor number is equal to this + minor_start */
    dev_t device_no;

    pid_t session_id;
    pid_t fg_pgrp;

    struct termios termios;
    struct tty_driver *driver;

    /* 
     * NOTE: These are labeled from the kernel's point of view.
     * Processes read from the 'output_buf', and write to the 'input_buf'
     */
    mutex_t inout_buf_lock;
    struct wait_queue in_wait_queue;

    struct char_buf output_buf;
    struct char_buf input_buf;
    int ret0; /* ^D marker */

    struct task *kernel_task;
};

#define TTY_INIT(tty) \
    { \
        .inout_buf_lock = MUTEX_INIT((tty).inout_buf_lock, "tty-inout-buf-lock"), \
        .in_wait_queue = WAIT_QUEUE_INIT((tty).in_wait_queue, "tty-in-wait-queue"), \
    }

static inline void tty_init(struct tty *tty)
{
    *tty = (struct tty)TTY_INIT(*tty);
}

void tty_driver_register(struct tty_driver *driver);

#endif

#endif
