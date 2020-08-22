#ifndef ARCH_X86_INCLUDE_ARCH_IO_H
#define ARCH_X86_INCLUDE_ARCH_IO_H

#include <protura/types.h>
#include <protura/compiler.h>

static __always_inline uint8_t read8(const volatile void *addr)
{
    uint8_t ret;
    asm volatile ("movb %1, %0"
            : "=q" (ret)
            : "m" (*(volatile uint8_t *)addr)
            : "memory");
    return ret;
}

static __always_inline uint16_t read16(volatile void *addr)
{
    uint16_t ret;
    asm volatile ("movw %1, %0"
            : "=r" (ret)
            : "m" (*(volatile uint16_t *)addr)
            : "memory");
    return ret;
}

static __always_inline uint32_t read32(volatile void *addr)
{
    uint32_t ret;
    asm volatile ("movl %1, %0"
            : "=r" (ret)
            : "m" (*(volatile uint32_t *)addr)
            : "memory");
    return ret;
}

static __always_inline void write8(volatile void *addr, uint8_t val)
{
    asm volatile ("movb %0, %1"
            :
            : "q" (val), "m" (*(volatile uint8_t *)addr)
            : "memory");
}

static __always_inline void write16(volatile void *addr, uint16_t val)
{
    asm volatile ("movw %0, %1"
            :
            : "r" (val), "m" (*(volatile uint16_t *)addr)
            : "memory");
}

static __always_inline void write32(volatile void *addr, uint32_t val)
{
    asm volatile ("movl %0, %1"
            :
            : "r" (val), "m" (*(volatile uint32_t *)addr)
            : "memory");
}

#endif
