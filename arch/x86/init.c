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

void arch_init(void)
{
    term_init();
    gdt_init();
    idt_init();
}

