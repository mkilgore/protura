/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_GDT_H
#define INCLUDE_ARCH_GDT_H

#ifndef ASM

#include <protura/compiler.h>
#include <protura/types.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    union {
        uint8_t  access;
        struct {
            uint8_t type :4;
            uint8_t des_type :1;
            uint8_t dpl :2;
            uint8_t present :1;
        };
    };
    
    union {
        uint8_t granularity;
        struct {
            uint8_t limit :4;
            uint8_t avl :1;
            uint8_t zero :1;
            uint8_t op_siz :1;
            uint8_t gran :1;
        };
    };
    uint8_t  base_high;
} __packed;

struct gdt_ptr {
    uint16_t limit;
    uint16_t base;
} __packed;


void init_gdt(void);

void gdt_flush(uint32_t);

#endif /* ASM */

#define _KERNEL_CS 0x08
#define _KERNEL_DS 0x10
#define _USER_CS   0x18
#define _USER_DS   0x20

#endif
