/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>

#include "irq_handler.h"
#include <arch/syscall.h>
#include <arch/gdt.h>
#include <arch/idt.h>

static struct idt_ptr idt_ptr;
static struct idt_entry idt_entries[256] = { {0} };

void idt_init(void)
{
    int i;

    idt_ptr.limit = sizeof(idt_entries) - 1;
    idt_ptr.base  = (uint32_t)&idt_entries;

    for (i = 0; i < 256; i++) {
        kprintf("Hand: %p ", irq_hands + i);
        IDT_SET_ENT(idt_entries[i], 0, _KERNEL_CS, (uint32_t)(irq_hands[i]), DPL_KERNEL);
    }
    IDT_SET_ENT(idt_entries[IRQ_SYSCALL], 1, _KERNEL_CS, (uint32_t)(irq_hands[IRQ_SYSCALL]), DPL_USER);

    idt_flush((uint32_t)&idt_ptr);
}

void irq_global_handler(struct idt_frame *iframe)
{
    kprintf("Got Int: %d\n", iframe->intno);    
    kprintf("Err: %d\n", iframe->err);
}

