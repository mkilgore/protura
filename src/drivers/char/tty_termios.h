/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef SRC_DRIVERS_CHAR_TTY_TERMIOS_H
#define SRC_DRIVERS_CHAR_TTY_TERMIOS_H

#include <protura/drivers/tty.h>

void tty_process_input(struct tty *tty);
void tty_process_output(struct tty *tty, const char *buf, size_t len);
void tty_line_buf_flush(struct tty *tty);

#endif
