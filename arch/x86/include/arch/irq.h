/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_IDQ_H
#define INCLUDE_ARCH_IDQ_H

#include <protura/types.h>
#include <protura/string.h>
#include <protura/compiler.h>
#include <arch/asm.h>
#include <arch/gdt.h>

#define DPL_USER 3
#define DPL_KERNEL 0

struct irq_frame {
    uint32_t edi, esi, ebp, oesp, ebx, edx, ecx, eax;
    uint16_t gs, pad1, fs, pad2, es, pad3, ds, pad4;

    uint32_t intno;
    uint32_t err;

    uint32_t eip;
    uint16_t cs, padding5;
    uint32_t eflags;

    uint32_t esp;
    uint16_t ss, padding6;
} __packed;

static inline void irq_frame_initalize(struct irq_frame *frame)
{
    memset(frame, 0, sizeof(*frame));
    frame->cs = _USER_CS | DPL_USER;

    frame->ss = _USER_DS | DPL_USER;
    frame->ds = _USER_DS | DPL_USER;
    frame->es = frame->ds;
    frame->fs = frame->ds;
    frame->gs = frame->ds;

    frame->eflags = EFLAGS_IF;
}

static inline void irq_frame_set_stack(struct irq_frame *frame, va_t stack)
{
    frame->esp = (uint32_t)stack;
}

static inline void irq_frame_set_ip(struct irq_frame *frame, va_t ip)
{
    frame->eip = (uint32_t)ip;
}

static inline void irq_frame_set_syscall_ret(struct irq_frame *frame, uint32_t ret)
{
    frame->eax = ret;
}

#define irq_disable() cli()

#endif
