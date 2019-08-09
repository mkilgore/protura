/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/snprintf.h>
#include <protura/scheduler.h>

int sys_setuid(uid_t uid)
{
    struct task *task = cpu_get_local()->current;

    task->uid = task->euid = uid;

    return 0;
}
