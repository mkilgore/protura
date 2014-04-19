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

#define GDT_ENTRY(base, limit, acs, gran) { \
        .limit_low   = ((limit) & 0xFFFF), \
        .base_low    = ((base) & 0xFFFF),  \
        .base_middle = (((base) >> 16) & 0xFF), \
        .base_high   = (((base) >> 24) & 0xFF), \
        .access      = (acs), \
        .granularity = (gran) \
    }

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __packed;

void gdt_init(void);

#endif /* ASM */

#define _KERNEL_CS_N 1
#define _KERNEL_DS_N 2
#define _USER_CS_N   3
#define _USER_DS_N   4

#define _KERNEL_CS (_KERNEL_CS_N << 3)
#define _KERNEL_DS (_KERNEL_DS_N << 3)
#define _USER_CS   (_USER_CS_N << 3)
#define _USER_DS   (_USER_DS_N << 3)

#endif
