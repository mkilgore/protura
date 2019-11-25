/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/basic_printf.h>
#include <protura/scheduler.h>
#include <protura/wait.h>

#include <arch/spinlock.h>
#include <arch/drivers/keyboard.h>
#include <arch/asm.h>
#include <protura/fs/char.h>
#include <protura/drivers/vt.h>
#include <protura/drivers/screen.h>
#include <protura/drivers/keyboard.h>
#include <protura/drivers/tty.h>
#include "vt_internal.h"

struct vt_printf_backbone {
    struct printf_backbone backbone;
    struct vt *vt;
};

/* Because this bypasses the TTY layer, no ONLCR processing happens. This is a
 * small hack to ensure newlines turn into CRLFs. */
static void __vt_write_with_crnl(struct vt_printf_backbone *vt, char ch)
{
    char cr = '\r';
    if (ch == '\n')
        vt_write(vt->vt, &cr, 1);

    vt_write(vt->vt, &ch, 1);
}

static void __vt_printf_putchar(struct printf_backbone *b, char ch)
{
    struct vt_printf_backbone *vt = container_of(b, struct vt_printf_backbone, backbone);
    __vt_write_with_crnl(vt, ch);
}

static void __vt_printf_putnstr(struct printf_backbone *b, const char *s, size_t len)
{
    struct vt_printf_backbone *vt = container_of(b, struct vt_printf_backbone, backbone);
    size_t i;

    for (i = 0; i < len; i++)
        __vt_write_with_crnl(vt, s[i]);
}

void vt_early_printf(struct kp_output *output, const char *fmt, va_list lst)
{
    struct vt_kp_output *vt_out = container_of(output, struct vt_kp_output, output);

    struct vt_printf_backbone backbone = {
        .backbone = {
            .putchar = __vt_printf_putchar,
            .putnstr = __vt_printf_putnstr,
        },
        .vt = vt_out->vt,
    };

    basic_printfv(&backbone.backbone, fmt, lst);
}
