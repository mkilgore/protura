/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_DRIVERS_PIC8259_H
#define INCLUDE_ARCH_DRIVERS_PIC8259_H

#include <protura/types.h>

#define PIC8259_IO_PIC1  0x20
#define PIC8259_IO_PIC2  0xA0

#define PIC8259_IRQ_SLAVE 2

#define PIC8259_IRQ0 0x20

#define PIC8259_EOI 0x20

void pic8259_enable_irq(int irq);
void pic8259_disable_irq(int irq);
void pic8259_send_eoi(int irq);

void pic8259_init(void);

uint8_t pic8259_read_master_isr(void);
uint8_t pic8259_read_slave_isr(void);

#endif
