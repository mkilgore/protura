/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_DRIVERS_QEMUDBG_H
#define INCLUDE_DRIVERS_QEMUDBG_H

#include <protura/types.h>
#include <protura/fs/char.h>
#include <protura/fs/file.h>

#define QEMUDBG_PORT 0xE9

extern struct file_ops qemu_dbg_file_ops;

void qemu_dbg_init(void);

#endif
