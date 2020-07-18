/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_MSR_H
#define INCLUDE_ARCH_MSR_H

static inline uint64_t x86_read_msr(uint32_t reg)
{
    uint32_t edx, eax;

    asm volatile ("rdmsr": "=d" (edx), "=a" (eax): "c" (reg));

    return ((uint64_t)edx << 32) + eax;
}

static inline void x86_write_msr(uint32_t reg, uint64_t val)
{
    uint32_t edx = val >> 32;
    uint32_t eax = val & 0xFFFFFFFF;

    asm volatile ("wrmsr": : "c" (reg), "d" (edx), "a" (eax));
}

#endif
