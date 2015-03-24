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

    uint8_t type :4;
    uint8_t des_type :1;
    uint8_t dpl :2;
    uint8_t present :1;

    uint8_t limit :4;
    uint8_t avl :1;
    uint8_t zero :1;
    uint8_t op_siz :1;
    uint8_t gran :1;

    uint8_t  base_high;
} __packed;


#define GDT_ENTRY(gdt_type, gdt_base, gdt_limit, gdt_dpl) \
    (struct gdt_entry)                                    \
    {                                                     \
        .limit_low   = (((gdt_limit) >> 12) & 0xFFFF),    \
        .base_low    = ((gdt_base) & 0xFFFF),             \
        .base_middle = (((gdt_base) >> 16) & 0xFF),       \
        .type = gdt_type,                                 \
        .des_type = 1,                                    \
        .dpl = gdt_dpl,                                   \
        .present = 1,                                     \
        .limit = ((gdt_limit) >> 28),                     \
        .avl = 0,                                         \
        .zero = 0,                                        \
        .op_siz = 1,                                      \
        .gran = 1,                                        \
        .base_high = (((gdt_base) >> 24) & 0xFF),         \
    }

#define GDT_ENTRY16(gdt_type, gdt_base, gdt_limit, gdt_dpl) \
    (struct gdt_entry)                                      \
    {                                                       \
        .limit_low = ((gdt_limit) & 0xFFFF),                \
        .base_low  = ((gdt_base) & 0xFFFF),                 \
        .base_middle = (((gdt_base) >> 16) & 0xFF),         \
        .type = gdt_type,                                   \
        .des_type = 1,                                      \
        .dpl = gdt_dpl,                                     \
        .present = 1,                                       \
        .limit = ((gdt_limit) >> 16),                       \
        .avl = 0,                                           \
        .zero = 0,                                          \
        .op_siz = 1,                                        \
        .gran = 0,                                          \
        .base_high = ((gdt_base) >> 24),                    \
    }

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __packed;

struct tss_entry {
    uint32_t next_tss;
    uint32_t *esp0;
    uint16_t ss0;
    uint16_t padding1;

    uint32_t *esp1;
    uint16_t ss1;
    uint16_t padding2;

    uint32_t *esp2;
    uint16_t ss2;
    uint16_t padding3;

    void *cr3;
    uint32_t *eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t *esp;
    uint32_t *ebp;
    uint32_t esi;
    uint32_t edi;

    uint16_t es;
    uint16_t padding4;
    uint16_t cs;
    uint16_t padding5;
    uint16_t ss;
    uint16_t padding6;
    uint16_t ds;
    uint16_t padding7;
    uint16_t fs;
    uint16_t padding8;
    uint16_t gs;
    uint16_t padding9;
    uint16_t ldt;
    uint16_t padding10;
    uint16_t t;
    uint16_t iomb;
};

/*
void gdt_tss_set(struct tss_entry *tss);
void gdt_tss_set_stack(void *kstack); */

static inline void gdt_flush(struct gdt_entry *gdt, uint16_t gdt_size)
{
    static struct gdt_ptr gptr;

    gptr.base = (uint32_t)gdt;
    gptr.limit = gdt_size;

    asm volatile("lgdt (%0)": : "r" (&gptr));
}

#endif /* ASM */

#define GDT_SEG_NULL_ASM() \
    .word 0, 0; \
    .byte 0, 0, 0, 0;

#define GDT_SEG_ASM(type, base, lim) \
    .word (((lim) >> 12) & 0xFFFF), ((base) & 0xFFFF); \
    .byte (((base) >> 16) & 0xFF), (0x90 | (type)), \
            (0xC0 | (((lim) >> 28) & 0xF)), (((base) >> 24) & 0xFF);

#define GDT_DPL_KERNEL 0x0
#define GDT_DPL_USER   0x3

#define GDT_TYPE_EXECUTABLE 0x8
#define GDT_TYPE_EXPAND     0x4
#define GDT_TYPE_CODE       0x4
#define GDT_TYPE_WRITABLE   0x2
#define GDT_TYPE_READABLE   0x2
#define GDT_TYPE_ACCESSED   0x1

/* GDT type-flag for 32-bit TSS */
#define GDT_STS_T32A   0x9

#define _GDT_NULL 0
#define _KERNEL_CS_N 1
#define _KERNEL_DS_N 2
#define _USER_CS_N 3
#define _USER_DS_N 4
#define _GDT_TSS_N 5
#define _CPU_VAR_N 6
#define GDT_ENTRIES 7

#define _KERNEL_CS (_KERNEL_CS_N << 3)
#define _KERNEL_DS (_KERNEL_DS_N << 3)
#define _USER_CS (_USER_CS_N << 3)
#define _USER_DS (_USER_DS_N << 3)
#define _GDT_TSS (_GDT_TSS_N << 3)
#define _CPU_VAR (_CPU_VAR_N << 3)

#endif
