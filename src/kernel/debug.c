/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/stdarg.h>
#include <protura/debug.h>
#include <protura/drivers/term.h>
#include <protura/spinlock.h>

#include <arch/backtrace.h>
#include <arch/asm.h>

struct kp_output {
    void (*print) (const char *fmt, va_list lst);
    const char *name;
};

static struct kp_output kp_output_table[CONFIG_KERNEL_LOG_MAX_OUTPUTS];

static spinlock_t kprintf_lock = SPINLOCK_INIT(0);

void kp_output_register(void (*print) (const char *fmt, va_list lst), const char *name)
{
    using_spinlock_nolog(&kprintf_lock) {
        int i;
        for (i = 0; i < CONFIG_KERNEL_LOG_MAX_OUTPUTS; i++) {
            if (!kp_output_table[i].print) {
                kp_output_table[i].print = print;
                kp_output_table[i].name = name;
                break;
            }
        }
    }
}

void kp_output_unregister(void (*print) (const char *fmt, va_list lst))
{
    using_spinlock_nolog(&kprintf_lock) {
        int i;
        for (i = 0; i < CONFIG_KERNEL_LOG_MAX_OUTPUTS; i++) {
            if (kp_output_table[i].print == print) {
                kp_output_table[i].print = NULL;
                kp_output_table[i].name = NULL;
                break;
            }
        }
    }
}

void kprintfv_internal(const char *fmt, va_list lst)
{
    using_spinlock_nolog(&kprintf_lock) {
        int i;
        for (i = 0; i < CONFIG_KERNEL_LOG_MAX_OUTPUTS; i++)
            if (kp_output_table[i].print)
                (kp_output_table[i].print) (fmt, lst);
    }
}

void kprintf_internal(const char *fmt, ...)
{
    va_list lst;
    va_start(lst, fmt);
    kprintfv_internal(fmt, lst);
    va_end(lst);
}

void __panicv(const char *s, va_list lst)
{
    cli();
    kprintfv_internal(s, lst);
    dump_stack();
    while (1)
        hlt();
}

void __panic(const char *s, ...)
{
    cli();
    va_list lst;
    va_start(lst, s);
    __panicv(s, lst);
    va_end(lst);
}

