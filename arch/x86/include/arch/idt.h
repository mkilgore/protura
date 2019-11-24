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
#include <protura/irq.h>
#include <protura/fs/procfs.h>

#include <arch/context.h>

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

enum irq_type {
    IRQ_INTERRUPT,
    IRQ_SYSCALL
};

enum {
    IRQF_SHARED,
};

struct irq_handler {
    const char *id;
    void (*callback)(struct irq_frame *, void *);
    void *param;
    enum irq_type type;
    flags_t flags;
    list_node_t entry;
};

#define IRQ_HANDLER_INIT(h, i, call, prm, typ, f) \
    { \
        .id = (i), \
        .callback = (call), \
        .param = (prm), \
        .type = (typ), \
        .flags = (f), \
        .entry = LIST_NODE_INIT((h).entry), \
    }

static inline void irq_handler_init(struct irq_handler *hand)
{
    *hand = (struct irq_handler) { .entry = LIST_NODE_INIT(hand->entry) };
}

/* Returns -1 if the registration fails */
int irq_register_handler(uint8_t irqno, struct irq_handler *);
int irq_register_callback(uint8_t irqno, void (*handler)(struct irq_frame *, void *param), const char *id, enum irq_type type, void *param, int flags);
int cpu_exception_register_callback(uint8_t exception_no, struct irq_handler *hand);
int x86_register_interrupt_handler(uint8_t irqno, struct irq_handler *hand);

void idt_flush(uint32_t);

void idt_init(void);

void irq_global_handler(struct irq_frame *);

extern const struct file_ops interrupts_file_ops;

#endif

#endif
