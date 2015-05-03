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
#include <arch/cpu.h>
#include <arch/task.h>
#include <arch/idt.h>

static struct idt_ptr idt_ptr;
static struct idt_entry idt_entries[256] = { {0} };

struct idt_identifier {
    atomic32_t count;
    const char *name;
    enum irq_type type;
    void (*handler)(struct idt_frame *);
};

static struct idt_identifier idt_ids[256] = { { ATOMIC32_INIT(0), 0 } };

atomic32_t intr_count = ATOMIC32_INIT(0);
int reschedule = 0;

void irq_register_callback(uint8_t irqno, void (*handler)(struct idt_frame *), const char *id, enum irq_type type)
{
    struct idt_identifier *ident = idt_ids + irqno;

    ident->handler = handler;
    ident->name = id;
    ident->type = type;
}

void idt_init(void)
{
    int i;

    idt_ptr.limit = sizeof(idt_entries) - 1;
    idt_ptr.base  = (uintptr_t)&idt_entries;

    kprintf("Limit: %d\n", idt_ptr.limit);
    kprintf("Base:  0x%x\n", idt_ptr.base);

    for (i = 0; i < 256; i++)
        IDT_SET_ENT(idt_entries[i], 0, _KERNEL_CS, (uint32_t)(irq_hands[i]), DPL_KERNEL);

    IDT_SET_ENT(idt_entries[INT_SYSCALL], 1, _KERNEL_CS, (uint32_t)(irq_hands[INT_SYSCALL]), DPL_USER);

    kprintf("syscall dpl: %d\n", idt_entries[INT_SYSCALL].dpl);

    idt_flush(((uintptr_t)&idt_ptr));
}

void interrupt_dump_stats(void (*print) (const char *fmt, ...))
{
    int k;
    (print) ("Interrupt stats:\n");
    for (k = 0; k < 256; k++) {
        /* Only display IRQ's that have a handler */
        if (!idt_ids[k].handler)
            continue;

        (print) (" %s(%02d) - %d\n", idt_ids[k].name, k, atomic32_get(&idt_ids[k].count));
    }
}

void irq_global_handler(struct idt_frame *iframe)
{
    struct idt_identifier *ident = idt_ids + iframe->intno;

    atomic32_inc(&ident->count);

    reschedule = 0;

    /* Only actual INTERRUPT types increment the intr_count */
    if (ident->type == IRQ_INTERRUPT)
        atomic32_inc(&intr_count);

    cpu_get_local()->current->context.frame = iframe;

    if (iframe->intno >= 0x20 && iframe->intno <= 0x31) {
        if (iframe->intno >= 40)
            outb(PIC8259_IO_PIC2, PIC8259_EOI);
        outb(PIC8259_IO_PIC1, PIC8259_EOI);
    }

    if (ident->handler != NULL)
        (ident->handler) (iframe);


    /* If this isn't an interrupt irq, then we don't do the clean-up. */
    if (ident->type != IRQ_INTERRUPT)
        return ;

    /* There's a possibility that interrupts are on, if this was a syscall.
     *
     * This point represents the end of the interrupt. From here we do clean-up
     * and exit to the running task */
    cli();
    atomic32_dec(&intr_count);

    /* If something set the reschedule flag and we're the last interrupt
     * (Meaning, we weren't fired while some other interrupt was going on, but
     * when a task was running), then we yield the current task, whcih
     * reschedules a new task to start running. */
    if (reschedule && atomic32_get(&intr_count) == 0) {
        reschedule = 0;
        task_yield();
    }
}

