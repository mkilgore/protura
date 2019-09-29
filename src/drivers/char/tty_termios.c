/*
 * Copyright (C) 2019 Matt Kilgore
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
#include <arch/asm.h>
#include <protura/fs/char.h>
#include <protura/fs/fcntl.h>
#include <protura/drivers/tty.h>
#include "tty_termios.h"

static void __send_input_char(struct tty *tty, char c)
{
    char_buf_write(&tty->output_buf, &c, 1);
    wait_queue_wake(&tty->in_wait_queue);
}

static void send_output_char(struct tty *tty, char c)
{
    const struct tty_driver *driver = tty->driver;
    driver->ops->write(tty, &c, 1);
}

static void output_post_process(struct tty *tty, const struct termios *termios, char c)
{
    if (c == '\n' && TERMIOS_ONLCR(termios)) {
        send_output_char(tty, '\r');
        return send_output_char(tty, '\n');
    }

    if (c == '\n' && TERMIOS_ONLRET(termios))
        return send_output_char(tty, '\r');

    if (c == '\r' && TERMIOS_OCRNL(termios))
        return send_output_char(tty, '\n');

    if (TERMIOS_OLCUC(termios))
        c = toupper(c);

    send_output_char(tty, c);
}

static void __tty_line_buf_flush(struct tty *tty, const struct termios *termios)
{
    size_t i;
    for (i = 0; i < tty->line_buf_pos; i++)
        __send_input_char(tty, tty->line_buf[i]);

    tty->line_buf_pos = 0;
}

void tty_line_buf_flush(struct tty *tty)
{
    using_mutex(&tty->lock)
        __tty_line_buf_flush(tty, &tty->termios);
}

static void __tty_line_buf_append(struct tty *tty, char c)
{
    if (tty->line_buf_pos < tty->line_buf_size) {
        tty->line_buf[tty->line_buf_pos] = c;
        tty->line_buf_pos++;
    }
}

static int __tty_line_buf_remove(struct tty *tty)
{
    if (tty->line_buf_pos > 0) {
        tty->line_buf_pos--;
        return 1;
    }

    return 0;
}

static void icanon_char(struct tty *tty, struct termios *termios, char c)
{
    if (c == termios->c_cc[VEOF]) {
        using_mutex(&tty->lock) {
            tty->ret0 = 1;
            wait_queue_wake(&tty->in_wait_queue);
        }
        return;
    }

    if (c == termios->c_cc[VERASE]) {
        using_mutex(&tty->lock) {
            if (__tty_line_buf_remove(tty) && TERMIOS_ECHO(termios) && TERMIOS_ECHOE(termios)) {
                output_post_process(tty, termios, '\b');
                output_post_process(tty, termios, ' ');
                output_post_process(tty, termios, '\b');
            }
        }
        return;
    }

    using_mutex(&tty->lock)
        __tty_line_buf_append(tty, c);

    if (c == '\n') {
        using_mutex(&tty->lock)
            __tty_line_buf_flush(tty, termios);
    }
}

static void echo_char(struct tty *tty, struct termios *termios, char c)
{
    /* ECHOE is handled at the ICANON level */
    if (TERMIOS_ECHOE(termios)
            && TERMIOS_ICANON(termios)
            && c == termios->c_cc[VERASE])
        return;

    output_post_process(tty, termios, c);
}

static void tty_pgrp_signal(struct tty *tty, int sig)
{
    pid_t pgrp;

    using_mutex(&tty->lock)
        pgrp = tty->fg_pgrp;

    if (pgrp) {
        kp(KP_TRACE, "tty: Sending %d to %d\n", sig, pgrp);
        scheduler_task_send_signal(-pgrp, sig, 0);
    }
}

static int isig_handle(struct tty *tty, struct termios *termios, char c)
{
    if (c == termios->c_cc[VINTR]) {
        tty_pgrp_signal(tty, SIGINT);
        return 1;
    } else if (c == termios->c_cc[VSUSP]) {
        tty_pgrp_signal(tty, SIGTSTP);
        return 1;
    }

    return 0;
}

/* Returns '1' if the input should be skipped */
static int input_preprocess(struct tty *tty, struct termios *termios, char *c)
{
    if (*c == '\r' && TERMIOS_IGNCR(termios))
        return 1;
    else if (*c == '\r' && TERMIOS_ICRNL(termios))
        *c = '\n';
    else if (*c == '\n' && TERMIOS_INLCR(termios))
        *c = '\r';

    if (TERMIOS_ISTRIP(termios))
        *c = *c & 0x7F;

    if (TERMIOS_IUCLC(termios))
        *c = tolower(*c);

    return 0;
}

void tty_process_input(struct tty *tty)
{
    const struct tty_driver *driver = tty->driver;
    char buf[32];
    size_t buf_len;
    size_t i;
    struct termios termios;

    using_mutex(&tty->lock)
        termios = tty->termios;

    buf_len = (driver->ops->read) (tty, buf, sizeof(buf));

    for (i = 0; i < buf_len; i++) {
        if (input_preprocess(tty, &termios, buf + i))
            continue;

        if (TERMIOS_ISIG(&termios) && isig_handle(tty, &termios, buf[i]))
            continue;

        if (TERMIOS_ECHO(&termios))
            echo_char(tty, &termios, buf[i]);

        if (TERMIOS_ICANON(&termios))
            icanon_char(tty, &termios, buf[i]);
        else {
            using_mutex(&tty->lock)
                __send_input_char(tty, buf[i]);
        }
    }
}

void tty_process_output(struct tty *tty, const char *buf, size_t buf_len)
{
    struct termios termios;

    using_mutex(&tty->lock)
        termios = tty->termios;

    size_t i;
    for (i = 0; i < buf_len; i++)
        output_post_process(tty, &termios, buf[i]);
}
