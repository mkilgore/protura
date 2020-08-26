/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/scheduler.h>
#include <protura/mm/kmalloc.h>
#include <protura/block/bcache.h>
#include <protura/kparam.h>
#include <protura/fs/sys.h>

static struct task *bdflushd_thread;

static int bdflush_delay_seconds = CONFIG_BDFLUSH_DELAY;
KPARAM("bdflush.delay", &bdflush_delay_seconds, KPARAM_INT);

static __noreturn int bdflushd_loop(void *ptr)
{
    while (1) {
        task_sleep_ms(bdflush_delay_seconds);

        sys_sync();
    }
}

void bdflush_init(void)
{
    bdflushd_thread = task_kernel_new("bdflushd", bdflushd_loop, NULL);
    scheduler_task_add(bdflushd_thread);
}
