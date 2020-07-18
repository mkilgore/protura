/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_DRIVERS_CONSOLE_H
#define INCLUDE_DRIVERS_CONSOLE_H

#include <protura/types.h>
#include <protura/fs/char.h>

#define CONSOLE_MAX CONFIG_CONSOLE_COUNT

void vt_console_kp_register(void);
void vt_console_kp_unregister(void);

void vt_console_early_init(void);
void vt_console_init(void);

void console_switch_vt(int new_vt);

#endif
