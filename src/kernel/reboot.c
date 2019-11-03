/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/errors.h>
#include <protura/reboot.h>
#include <arch/reset.h>

int sys_reboot(int magic1, int magic2, int cmd)
{
    if (magic1 != PROTURA_REBOOT_MAGIC1 || magic2 != PROTURA_REBOOT_MAGIC2)
        return -EINVAL;

    switch (cmd) {
    case PROTURA_REBOOT_RESTART:
        kp(KP_WARNING, "System is rebooting!!!!");
        system_reboot();
        break;

    default:
        return -EINVAL;
    }
}
