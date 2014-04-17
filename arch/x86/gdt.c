/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>

#include <arch/gdt.h>

static struct gdt_entry gdt_entries[5];
static struct gdt_ptr   gdt_ptr;

static void gdt_set_gate(uint32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
    struct gdt_entry *ent = gdt_entries + num;

    ent->base_low = base & 0xFFFF;
    ent->base_middle = (base >> 16) & 0xFF;
    ent->base_high = (base >> 24) & 0xFF;

    ent->limit_low = limit & 0xFFFF;
    ent->granularity |= gran & 0xF0;
    ent->access = access;
}

void init_gdt(void)
{
    gdt_ptr.limit = (sizeof(struct gdt_entry) * 5) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    gdt_set_gate(0, 0, 0, 0, 0);
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    gdt_flush((uint32_t)&gdt_ptr);
}

