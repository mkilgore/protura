/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#include <protura/types.h>

#include <arch/asm.h>
#include <arch/drivers/pic8259.h>

static uint16_t irqmask = 0xFFFF & ~(1<<PIC8259_IRQ_SLAVE);

static void pic_set_mask(void)
{
    int eflags;
    eflags = eflags_read();
    cli();

    outb(PIC8259_IO_PIC1 + 1, irqmask);
    outb(PIC8259_IO_PIC2 + 1, irqmask >> 8);

    eflags_write(eflags);
}

void pic8259_enable_irq(int irq)
{
    irqmask = irqmask & ~(1 << irq);
    pic_set_mask();
}

void pic8259_init(void)
{
    outb(PIC8259_IO_PIC1 + 1, 0xFF);
    outb(PIC8259_IO_PIC2 + 1, 0xFF);

    outb(PIC8259_IO_PIC1, 0x11);
    outb(PIC8259_IO_PIC2, 0x11);

    outb(PIC8259_IO_PIC1 + 1, PIC8259_IRQ0);
    outb(PIC8259_IO_PIC2 + 1, PIC8259_IRQ0 + 8);

    outb(PIC8259_IO_PIC1 + 1, 1 << PIC8259_IRQ_SLAVE);
    outb(PIC8259_IO_PIC2 + 1, PIC8259_IRQ_SLAVE);

    outb(PIC8259_IO_PIC1 + 1, 0x01);
    outb(PIC8259_IO_PIC2 + 1, 0x01);

    pic_set_mask();
}

