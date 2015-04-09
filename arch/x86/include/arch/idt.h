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
#include <protura/atomic.h>

#include <arch/context.h>

#define DPL_USER 3
#define DPL_KERNEL 0

#define STS_TG32 0xF
#define STS_IG32 0xE

struct idt_entry {
    uint16_t base_low;
    uint16_t cs;
    uint8_t  zero;
    uint8_t  type :4;
    uint8_t  s :1;
    uint8_t  dpl :2;
    uint8_t  p :1;
    uint16_t  base_high;
} __packed;

#define IDT_SET_ENT(ent, istrap, sel, off, d) do { \
        (ent).base_low = (uint32_t)(off) & 0xFFFF; \
        (ent).base_high = ((uint32_t)(off) >> 16) & 0xFFFF; \
        (ent).cs = sel; \
        (ent).zero = 0; \
        (ent).type = (istrap) ? STS_TG32 : STS_IG32; \
        (ent).s = 0; \
        (ent).dpl = (d); \
        (ent).p = 1; \
    } while (0)

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __packed;

struct idt_frame {
    struct x86_regs regs;

    uint32_t intno;
    uint32_t err;

    uint32_t eip;
    uint16_t cs, padding5;
    uint32_t eflags;

    uint32_t esp;
    uint16_t ss, padding6;
} __packed;

void idt_flush(uint32_t);

void idt_init(void);

void irq_global_handler(struct idt_frame *);
void irq_register_callback(uint8_t irqno, void (*callback)(struct idt_frame *), const char *id);

void interrupt_dump_stats(void (*print) (const char *fmt, ...) __printf(1, 2));

extern atomic32_t intr_count;
extern int reschedule;

#endif

#endif
