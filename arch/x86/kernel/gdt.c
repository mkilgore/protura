/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>

#include <arch/paging.h>
#include "gdt_flush.h"
#include <arch/asm.h>
#include <arch/gdt.h>

static struct gdt_ptr gdt_ptr;

static struct gdt_entry gdt_entries[] = {
    [_GDT_NULL] = { 0 },
    [_KERNEL_CS_N] = GDT_ENTRY(GDT_TYPE_EXECUTABLE | GDT_TYPE_READABLE, 0, 0xFFFFFFFF, GDT_DPL_KERNEL),
    [_KERNEL_DS_N] = GDT_ENTRY(GDT_TYPE_WRITABLE, 0, 0xFFFFFFFF, GDT_DPL_KERNEL),
    [_USER_CS_N] = GDT_ENTRY(GDT_TYPE_EXECUTABLE | GDT_TYPE_READABLE, 0, 0xFFFFFFFF, GDT_DPL_USER),
    [_USER_DS_N] = GDT_ENTRY(GDT_TYPE_WRITABLE, 0, 0xFFFFFFFF, GDT_DPL_USER),
    [_CPU_VAR_N] = { 0 },
    [_GDT_TSS_N] = { 0 },
};

static struct tss_entry tss;

void gdt_tss_set(struct tss_entry *tss)
{
    gdt_entries[_GDT_TSS_N] = (struct gdt_entry)GDT_ENTRY16(GDT_STS_T32A, (uintptr_t)tss, sizeof(*tss) - 1, GDT_DPL_KERNEL);
    gdt_entries[_GDT_TSS_N].des_type = 0;

    ltr(_GDT_TSS);
}

void gdt_tss_set_stack(void *kstack)
{
    tss.ss0 = _KERNEL_DS;
    tss.esp0 = (uint32_t)kstack;

    tss.cs = _KERNEL_CS;
    tss.ss = _KERNEL_DS;
    tss.ds = _KERNEL_DS;
    tss.es = _KERNEL_DS;
    tss.fs = _KERNEL_DS;
    tss.gs = _CPU_VAR;

    tss.iomb = sizeof(tss);
    gdt_tss_set(&tss);
}

void gdt_set_cpu_local(void *var, size_t size)
{
    gdt_entries[_CPU_VAR_N] = GDT_ENTRY(GDT_TYPE_WRITABLE, var, size, 0);
}

void gdt_init(void)
{
    gdt_ptr.limit = (sizeof(gdt_entries)) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    gdt_flush((uint32_t)&gdt_ptr);
}

