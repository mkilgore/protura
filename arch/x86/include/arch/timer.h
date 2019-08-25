/*
 * Copyright (C) 2018 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_TIMER_H
#define INCLUDE_ARCH_TIMER_H

#include <protura/types.h>

uint32_t timer_get_ms(void);
uint32_t timer_get_ticks(void);
uint32_t sys_clock(void);

#define TIMER_TICKS_PER_SEC 1000

#endif
