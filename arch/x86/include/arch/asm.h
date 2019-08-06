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

#define DEFINE_REG_ACCESS_FUNCTIONS(prefix, arg, base) \
    static inline void prefix##_outb(arg, uint16_t reg, uint8_t value) { \
        outb((base) + reg, value); \
    } \
    static inline void prefix##_outw(arg, uint16_t reg, uint16_t value) { \
        outw((base) + reg, value); \
    } \
    static inline void prefix##_outl(arg, uint16_t reg, uint32_t value) { \
        outl((base) + reg, value); \
    } \
    static inline uint8_t prefix##_inb(arg, uint8_t reg) { \
        return inb((base) + reg); \
    } \
    static inline uint16_t prefix##_inw(arg, uint8_t reg) { \
        return inw((base) + reg); \
    } \
    static inline uint32_t prefix##_inl(arg, uint8_t reg) { \
        return inl((base) + reg); \
    }

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
    asm volatile("outw %1, %0": : "dN" (port), "a" (value));
}

static __always_inline uint32_t inl(uint16_t port)
{
    uint32_t ret;
    asm volatile("inl %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}

static __always_inline void outl(uint16_t port, uint32_t value)
{
    asm volatile("outl %1, %0": : "dN" (port), "a" (value));
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

static __always_inline uint32_t xchg(volatile void *addr, uint32_t val)
{
    volatile uint32_t *ptr = addr;
    uint32_t result;

    asm volatile(LOCK_PREFIX" xchgl %0, %1\n":
                 "+m" (*ptr), "=a" (result) :
                 "1" (val) :
                 "cc", "memory");

    return result;
}

static __always_inline uint32_t cmpxchg(volatile void *addr, uint32_t cmp, uint32_t swap)
{
    volatile uint32_t *ptr = addr;
    uint32_t ret;

    asm volatile(LOCK_PREFIX " cmpxchgl %2, %0\n"
            : "+m" (*ptr), "=a" (ret)
            : "r" (swap), "1" (cmp)
            : "memory");

    return ret;
}

/* 'xchg' but for pointers. atomically stores the pointer value 'new' into the
 * location pointed too by 'old'. */
static __always_inline void *atomic_ptr_swap(volatile void *addr, void *new)
{
    return (void *)xchg(addr, (uint32_t)new);
}

static __always_inline uint64_t rdtsc(void)
{
   uint32_t a, d;

   asm volatile("rdtsc" : "=a" (a), "=d" (d));

   return ((uint64_t)a) | (((uint64_t)d) << 32);;
}

#endif
