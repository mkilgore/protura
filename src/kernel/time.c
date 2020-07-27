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
#include <protura/atomic.h>
#include <protura/mm/vm.h>
#include <protura/mm/user_check.h>
#include <protura/fs/procfs.h>
#include <arch/timer.h>
#include <protura/time.h>

time_t current_uptime = 0;
time_t boot_time = 0;

void protura_uptime_inc(void)
{
    return atomic32_inc((atomic32_t *)&current_uptime);
}

time_t protura_uptime_get(void)
{
    return atomic32_get((atomic32_t *)&current_uptime);
}

void protura_uptime_reset(void)
{
    atomic32_set((atomic32_t *)&current_uptime, 0);
}

void protura_boot_time_set(time_t t)
{
    boot_time = t;
}

time_t protura_boot_time_get(void)
{
    return boot_time;
}

time_t protura_current_time_get(void)
{
    return protura_uptime_get() + protura_boot_time_get();
}

uint32_t protura_uptime_get_ms(void)
{
    return timer_get_ms();
}

static int protura_uptime_read(void *page, size_t page_size, size_t *len)
{
    *len = snprintf(page, page_size, "%ld\n", protura_uptime_get());

    return 0;
}

static int protura_boot_time_read(void *page, size_t page_size, size_t *len)
{
    *len = snprintf(page, page_size, "%ld\n", protura_boot_time_get());

    return 0;
}

static int protura_current_time_read(void *page, size_t page_size, size_t *len)
{
    *len = snprintf(page, page_size, "%ld\n", protura_current_time_get());

    return 0;
}

struct procfs_entry_ops uptime_ops = {
    .readpage = protura_uptime_read,
};

struct procfs_entry_ops boot_time_ops = {
    .readpage = protura_boot_time_read,
};

struct procfs_entry_ops current_time_ops = {
    .readpage = protura_current_time_read,
};

int sys_time(struct user_buffer t)
{
    return user_copy_from_kernel(t, protura_current_time_get());
}

int sys_gettimeofday(struct user_buffer tv, struct user_buffer tz)
{
    uint32_t tick = timer_get_ticks();
    struct timeval tmp;

    tmp.tv_sec = tick / TIMER_TICKS_PER_SEC;
    tmp.tv_usec = ((uint64_t)(tick % (TIMER_TICKS_PER_SEC)) * 1000000 / TIMER_TICKS_PER_SEC);

    return user_copy_from_kernel(tv, tmp);
}
