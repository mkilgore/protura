/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/stdarg.h>
#include <protura/types.h>
#include <protura/basic_printf.h>

#include <arch/init.h>
#include <drivers/term.h>
#include <arch/drivers/com1_debug.h>

static void arch_putchar(struct printf_backbone *b, char ch)
{
    if (kernel_is_booting)
        term_putchar(ch);
    com1_putchar(ch);
}

static void arch_putnstr(struct printf_backbone *b, const char *s, size_t len)
{
    if (kernel_is_booting)
        term_putnstr(s, len);
    com1_putnstr(s, len);
}

struct printf_backbone_arch {
    struct printf_backbone backbone;
};

void arch_kprintfv(const char *fmt, va_list lst)
{
    struct printf_backbone_arch arch = {
        .backbone = {
            .putchar = arch_putchar,
            .putnstr = arch_putnstr,
        },
    };

    basic_printfv(&arch.backbone, fmt, lst);

    return ;
}


void arch_kprintf(const char *fmt, ...)
{
    va_list lst;
    va_start(lst, fmt);
    arch_kprintfv(fmt, lst);
    va_end(lst);
    return ;
}

