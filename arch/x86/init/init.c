/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <drivers/term.h>

#include <arch/gdt.h>
#include <arch/idt.h>
#include <arch/init.h>
#include <arch/drivers/pic8259.h>
#include <arch/drivers/pic8259_timer.h>

void arch_init(void)
{
    term_init();
    gdt_init();
    idt_init();
    pic8259_init();
    pic8259_timer_init(50);
}

