/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ASM_H
#define INCLUDE_ASM_H

#include <stdint.h>

#include "compiler.h"

__always_inline uint8_t inb(uint16_t port)
{
    uint8_t data;

    asm volatile("in %1, %0" : "=a" (data) : "d" (port));
    return data;
}

__always_inline void insl(int32_t port, void *addr, int32_t cnt)
{
    asm volatile("cld; rep insl" :
                 "=D" (addr), "=c" (cnt) :
                 "d" (port), "0" (addr), "1" (cnt) :
                 "memory", "cc");
}

#endif

