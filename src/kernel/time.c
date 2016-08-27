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
#include <protura/mm/user_ptr.h>
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

int protura_uptime_read(void *page, size_t page_size, size_t *len)
{
    *len = snprintf(page, page_size, "%d\n", protura_uptime_get());

    return 0;
}

int protura_boot_time_read(void *page, size_t page_size, size_t *len)
{
    *len = snprintf(page, page_size, "%d\n", protura_boot_time_get());

    return 0;
}

int protura_current_time_read(void *page, size_t page_size, size_t *len)
{
    *len = snprintf(page, page_size, "%d\n", protura_current_time_get());

    return 0;
}

int sys_time(time_t *t)
{
    int ret;

    ret = user_check_region(t, sizeof(*t), F(VM_MAP_WRITE));
    if (ret)
        return ret;

    *t = protura_current_time_get();

    return 0;
}

