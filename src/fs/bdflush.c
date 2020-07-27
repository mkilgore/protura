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
#include <protura/fs/block.h>
#include <protura/fs/sys.h>

static struct task *bdflushd_thread;

static __noreturn int bdflushd_loop(void *ptr)
{
    while (1) {
        task_sleep_ms(CONFIG_BDFLUSH_DELAY);

        sys_sync();
    }
}

void block_cache_init(void)
{
    bdflushd_thread = task_kernel_new("bdflushd", bdflushd_loop, NULL);
    scheduler_task_add(bdflushd_thread);
}
