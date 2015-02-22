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

#endif
