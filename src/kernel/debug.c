/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/stdarg.h>
#include <protura/debug.h>
#include <protura/spinlock.h>

#include <arch/backtrace.h>
#include <arch/reset.h>
#include <arch/asm.h>

static spinlock_t kprintf_lock = SPINLOCK_INIT(0);
static list_head_t kp_output_list = LIST_HEAD_INIT(kp_output_list);

void kp_output_register(struct kp_output *output)
{
    using_spinlock_nolog(&kprintf_lock)
        list_add_tail(&kp_output_list, &output->node);
}

void kp_output_unregister(struct kp_output *rm_output)
{
    using_spinlock_nolog(&kprintf_lock) {
        struct kp_output *output;

        list_foreach_entry(&kp_output_list, output, node) {
            if (output == rm_output) {
                list_del(&rm_output->node);
                break;
            }
        }
    }
}

void kprintfv_internal(const char *fmt, va_list lst)
{
    struct kp_output *output;

    using_spinlock_nolog(&kprintf_lock)
        list_foreach_entry(&kp_output_list, output, node)
            (output->print) (output, fmt, lst);
}

void kprintf_internal(const char *fmt, ...)
{
    va_list lst;
    va_start(lst, fmt);
    kprintfv_internal(fmt, lst);
    va_end(lst);
}

int reboot_on_panic = 0;

void __panicv(const char *s, va_list lst)
{
    cli();
    kprintfv_internal(s, lst);
    dump_stack(KP_ERROR);

    if (reboot_on_panic)
        system_reboot();

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

