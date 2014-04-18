/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_IDT_H
#define INCLUDE_ARCH_IDT_H

#ifndef ASM

#include <protura/compiler.h>
#include <protura/types.h>

struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  zero;
    uint8_t  type;
    uint8_t  base_high;
} __packed;

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __packed;

struct idt_frame {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t oesp; /* Ignore */
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

    uint16_t gs;
    uint16_t pad1;
    uint16_t fs;
    uint16_t pad2;
    uint16_t es;
    uint16_t pad3;
    uint16_t ds;
    uint16_t pad4;

    uint32_t intno;

    uint32_t err;
    uint32_t eip;
    uint16_t cs;
    uint16_t padding5;
    uint32_t eflags;

    uint32_t esp;
    uint16_t ss;
    uint16_t padding6;
} __packed;

void idt_flush(uint32_t);

void idt_init(void);

void isr_handler(struct idt_frame *);

#endif

#endif
