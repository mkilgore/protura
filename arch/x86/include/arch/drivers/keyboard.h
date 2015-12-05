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

enum {
    KEY_HOME = 0x4700,
    KEY_UP   = 0x4800,
    KEY_PGUP = 0x4900,
    KEY_LEFT = 0x4B00,
    KEY_RIGHT = 0x4D00,
    KEY_END   = 0x4F00,
    KEY_DOWN  = 0x5000,
    KEY_PGDN  = 0x5100,
    KEY_INSERT = 0x5200,
    KEY_DELETE = 0x5300,
};

void arch_keyboard_init(void);
int arch_keyboard_has_char(void);
int arch_keyboard_get_char(void);

void arch_keyboard_wakeup_add(struct task *);
void arch_keyboard_wakeup_remove(struct task *);

#endif
