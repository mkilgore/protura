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

#if __KERNEL__

extern struct file_ops tty_file_ops;
void tty_init(void);

struct tty_driver;
struct tty;
struct file;

struct tty_ops {
    int (*has_chars) (struct tty_driver *);
    int (*read) (struct tty_driver *, char *, size_t);
    int (*write) (struct tty_driver *, const char *, size_t size);
};

struct tty_driver {
    const char *name;
    uint16_t minor_start;
    uint16_t minor_end;

    struct termios termios;
    struct tty_ops *ops;

    mutex_t inout_buf_lock;
    struct wait_queue in_wait_queue;

    /* 
     * NOTE: These are labeled from the kernel's point of view.
     * Processes read from the 'output_buf', and write to the 'input_buf'
     */
    struct char_buf output_buf;
    struct char_buf input_buf;
    int ret0; /* ^D marker */


    struct task *kernel_task;

    list_node_t tty_driver_node;
};

void tty_register(struct tty_driver *driver);

#endif

#endif
