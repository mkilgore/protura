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
#include <protura/compiler.h>

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

#endif
