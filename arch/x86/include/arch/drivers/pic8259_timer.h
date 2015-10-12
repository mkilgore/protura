/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_DRIVERS_PIC8259_TIMER_H
#define INCLUDE_ARCH_DRIVERS_PIC8259_TIMER_H

#include <protura/types.h>

#define PIC8259_TIMER_IO      0x40

#define PIC8259_TIMER_FREQ    1193182
#define PIC8259_TIMER_DIV(x)  ((PIC8259_TIMER_FREQ) / (x))

#define PIC8259_TIMER_MODE    (PIC8259_TIMER_IO + 3)
#define PIC8259_TIMER_SEL0    0x00
#define PIC8259_TIMER_RATEGEN 0x04
#define PIC8259_TIMER_16BIT   0x30

#define PIC8259_TIMER_IRQ     0x00 + 0x20

#define TIMER_TICKS_PER_SEC 1000

void pic8259_timer_init(void);
uint32_t timer_get_ticks(void);
uint32_t sys_clock(void);

#endif
