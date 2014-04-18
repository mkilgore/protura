/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>

#include "gdt_flush.h"
#include <arch/gdt.h>

static struct gdt_ptr gdt_ptr;

static struct gdt_entry gdt_entries[5] = {
    GDT_ENTRY(0, 0, 0, 0),
    GDT_ENTRY(0, 0xFFFFFFFF, 0x9A, 0xCF),
    GDT_ENTRY(0, 0xFFFFFFFF, 0x92, 0xCF),
    GDT_ENTRY(0, 0xFFFFFFFF, 0xFA, 0xCF),
    GDT_ENTRY(0, 0xFFFFFFFF, 0xF2, 0xCF)
};

void gdt_init(void)
{
    gdt_ptr.limit = (sizeof(gdt_entries));
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    gdt_flush((uint32_t)&gdt_ptr);
}

