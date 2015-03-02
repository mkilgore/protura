/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/atomic.h>
#include <mm/memlayout.h>

#include "irq_handler.h"
#include <arch/asm.h>
#include <arch/syscall.h>
#include <arch/drivers/pic8259.h>
#include <arch/gdt.h>
#include <arch/idt.h>

static struct idt_ptr idt_ptr;
static struct idt_entry idt_entries[256] = { {0} };

static void (*irq_handlers[256])(struct idt_frame *);

struct idt_identifier {
    atomic32_t count;
    const char *name;
};

static struct idt_identifier idt_ids[256] = { { ATOMIC32_INIT(0), 0 } };

void irq_register_callback(uint8_t irqno, void (*handler)(struct idt_frame *), const char *id)
{
    irq_handlers[irqno] = handler;
    idt_ids[irqno].name = id;
}

void idt_init(void)
{
    int i;

    idt_ptr.limit = sizeof(idt_entries) - 1;
    idt_ptr.base  = (uintptr_t)&idt_entries;

    kprintf("Limit: %d\n", idt_ptr.limit);
    kprintf("Base:  %x\n", idt_ptr.base);

    for (i = 0; i < 256; i++)
        IDT_SET_ENT(idt_entries[i], 0, _KERNEL_CS, (uint32_t)(irq_hands[i]), DPL_KERNEL);

    IDT_SET_ENT(idt_entries[IRQ_SYSCALL], 0, _KERNEL_CS, (uint32_t)(irq_hands[IRQ_SYSCALL]), DPL_USER);

    idt_flush(((uintptr_t)&idt_ptr));
}

void interrupt_dump_stats(void (*print) (const char *fmt, ...))
{
    int k;
    (print) ("Interrupt stats:\n");
    for (k = 0; k < 256; k++) {
        /* Only display IRQ's that have a handler */
        if (!irq_handlers[k])
            continue;

        (print) (" %s(%02d) - %d\n", idt_ids[k].name, k, atomic32_get(&idt_ids[k].count));
    }
}

void irq_global_handler(struct idt_frame *iframe)
{
    atomic32_inc(&idt_ids[iframe->intno].count);

    if (iframe->intno >= 0x20 && iframe->intno <= 0x31) {
        if (iframe->intno >= 40)
            outb(PIC8259_IO_PIC2, PIC8259_EOI);
        outb(PIC8259_IO_PIC1, PIC8259_EOI);
    }
    if (irq_handlers[iframe->intno] != NULL)
        (irq_handlers[iframe->intno]) (iframe);
}

