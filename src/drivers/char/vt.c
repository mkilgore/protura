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

/* Because this bypasses the TTY layer, no ONLCR processing happens. This is a
 * small hack to ensure newlines turn into CRLFs. */
static void vt_write_with_crnl(struct vt *vt, char ch)
{
    char cr = '\r';
    if (ch == '\n')
        vt_write(vt, &cr, 1);

    vt_write(vt, &ch, 1);
}

void vt_early_print(struct kp_output *output, const char *str)
{
    struct vt_kp_output *vt_out = container_of(output, struct vt_kp_output, output);
    struct vt *vt = vt_out->vt;

    for (; *str; str++)
        vt_write_with_crnl(vt, *str);
}
