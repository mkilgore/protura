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
#include <config/autoconf.h>
#include <mm/palloc.h>

#include <arch/cpu.h>
#include <arch/asm.h>
#include <arch/idt.h>
#include <arch/task.h>
#include <arch/drivers/pic8259.h>
#include <arch/drivers/pic8259_timer.h>

static atomic32_t ticks;

static void timer_callback(struct irq_frame *frame)
{
    atomic32_inc(&ticks);

    if ((atomic32_get(&ticks) % (TIMER_TICKS_PER_SEC / CONFIG_TASKSWITCH_PER_SEC)) == 0)
        cpu_get_local()->reschedule = 1;

    if ((atomic32_get(&ticks) % (TIMER_TICKS_PER_SEC / 2)) == 0)
        kp(KP_NORMAL, "palloc: %d free pages\n", palloc_free_page_count());
}

uint32_t timer_get_ticks(void)
{
    return atomic32_get(&ticks);
}

uint32_t sys_clock(void)
{
    return timer_get_ticks();
}

void pic8259_timer_init(void)
{
    outb(PIC8259_TIMER_MODE,
            PIC8259_TIMER_SEL0
          | PIC8259_TIMER_RATEGEN
          | PIC8259_TIMER_16BIT);
    outb(PIC8259_TIMER_IO,
            PIC8259_TIMER_DIV(TIMER_TICKS_PER_SEC) % 256);
    outb(PIC8259_TIMER_IO,
            PIC8259_TIMER_DIV(TIMER_TICKS_PER_SEC) / 256);

    pic8259_enable_irq(PIC8259_TIMER_IRQ);

    irq_register_callback(PIC8259_IRQ0, timer_callback, "PIC 8259 Timer", IRQ_INTERRUPT);
}

