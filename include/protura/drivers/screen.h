/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_DRIVERS_SCREEN_H
#define INCLUDE_DRIVERS_SCREEN_H

#include <protura/types.h>
#include <protura/fs/char.h>

void screen_init(void);
int screen_file_write(struct file *filp, const void *buf, size_t len);

extern struct file_ops screen_file_ops;

#endif
