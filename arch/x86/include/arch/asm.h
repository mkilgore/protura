/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_ASM_H
#define INCLUDE_ARCH_ASM_H

#include <protura/types.h>
#include <protura/compiler.h>

#define LOCK_PREFIX "lock "

static __always_inline void outb(uint16_t port, uint8_t value)
{
    asm volatile("outb %1, %0" : : "dN" (port), "a" (value));
}

static __always_inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile("inb %1, %0": "=a" (ret) : "dN" (port));
    return ret;
}

static __always_inline uint16_t inw(uint16_t port)
{
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}

static __always_inline void outw(uint16_t port, uint16_t value)
{
    asm volatile("outw %1, $0": : "dN" (port), "a" (value));
}

static __always_inline void outsl(uint16_t port, void *data, int32_t count)
{
    asm volatile("cld; rep outsl": "=S" (data), "=c" (count)
            : "d" (port), "0" (data), "1" (count)
            : "cc");
}

static __always_inline void insl(uint16_t port, void *data, int32_t count)
{
    asm volatile("cld; rep insl": "=D" (data), "=c" (count)
            : "d" (port), "0" (data), "1" (count)
            : "memory", "cc");

}

static __always_inline void hlt(void)
{
    asm volatile("hlt");
}

static __always_inline void cli(void)
{
    asm volatile("cli");
}

static __always_inline void sti(void)
{
    asm volatile("sti");
}

static __always_inline void ltr(uint16_t tss_seg)
{
    asm volatile("ltr %0\n"
                 : : "r" (tss_seg));
}

static __always_inline uint32_t eflags_read(void)
{
    uint32_t flags;
    asm volatile("pushfl\n"
                 "popl %0\n" : "=a" (flags));
    return flags;
}

#define EFLAGS_IF 0x00000200

static __always_inline void eflags_write(uint32_t flags)
{
    asm volatile("pushl %0\n"
                 "popfl\n" : : "a" (flags));
}

static __always_inline uint32_t xchg(volatile uint32_t *addr, uint32_t val)
{
    uint32_t result;

    asm volatile(LOCK_PREFIX" xchgl %0, %1\n":
                 "+m" (*addr), "=a" (result) :
                 "1" (val) :
                 "cc");
    return result;
}

#endif
