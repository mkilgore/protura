/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>

#include "idt_table.h"
#include <arch/gdt.h>
#include <arch/idt.h>

static struct idt_ptr idt_ptr;

static struct idt_entry idt_entries[256] = { {0} };

static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags)
{
    struct idt_entry *ent = idt_entries + num;

    ent->base_low = base & 0xFFFF;
    ent->base_high = (base >> 16) & 0xFFFF;

    ent->sel = sel;
    ent->zero = 0;

    ent->type = flags;
}

void idt_init(void)
{
    void (*func[32])(void) = {
        isr0, isr1, isr2, isr3, isr4, isr5,
        isr6, isr7, isr8, isr9, isr10, isr11,
        isr12, isr13, isr14, isr15, isr16, isr17,
        isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29,
        isr30, isr31
    };
    int i;

    idt_ptr.limit = sizeof(idt_entries);
    idt_ptr.base  = (uint32_t)&idt_entries;

    for (i = 0; i < 32; i++)
        idt_set_gate(i, (uint32_t)func[i], _KERNEL_CS, 0x8E);

    idt_flush((uint32_t)&idt_ptr);
}

void isr_handler(struct idt_frame *iframe)
{
    kprintf("Got Int: %d\n", iframe->intno);    
}

