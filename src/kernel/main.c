/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/multiboot.h>

#include <arch/init.h>


int cmain(uint32_t magic, struct multiboot_info *addr)
{

    arch_init();

    kprintf("Booting Protura...\n");

    asm volatile("sti":::"memory");

    while (1);

    return 0;
}

