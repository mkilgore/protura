/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>

#include <arch/init.h>


int cmain(uint32_t magic, uint32_t addr)
{
    int i = 250;
    void *p =(void *)0xDEADBEEF;
    char *s = "Test2";

    arch_init();

    kprintf("Hello! :D\n");

    kprintf("Test int: %d\n ptr: %p\n str: %s\n", i, p, s);

    asm volatile("int $0x3");
    asm volatile("int $0x4");

    return 0;
}

