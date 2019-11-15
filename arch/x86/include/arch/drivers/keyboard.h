/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_DRIVERS_KEYBOARD_H
#define INCLUDE_ARCH_DRIVERS_KEYBOARD_H

#include <protura/config/autoconf.h>
#include <protura/types.h>
#include <arch/spinlock.h>
#include <protura/atomic.h>
#include <protura/char_buf.h>
#include <arch/task.h>
#include <protura/scheduler.h>
#include <protura/work.h>

void arch_keyboard_init(void);
void arch_keyboard_set_leds(int led_flags);

#endif
