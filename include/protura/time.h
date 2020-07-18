/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_TIME_H
#define INCLUDE_PROTURA_TIME_H

#include <protura/types.h>

struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

/* Number of days from year 0 to the start of the Unix Epoch, 1970-01-01 */
#define TIME_DAYS_TO_EPOCH 719499

time_t protura_uptime_get(void);
uint32_t protura_uptime_get_ms(void);
void protura_uptime_inc(void);

 /*
  * Used when we read the RTC time on boot, if we have one. This allowes us to
  * sync the uptime to starting aproximately when the RTC time was.
  */
void protura_uptime_reset(void);

time_t protura_boot_time_get(void);
void protura_boot_time_set(time_t t);

time_t protura_current_time_get(void);

extern struct procfs_entry_ops uptime_ops;
extern struct procfs_entry_ops boot_time_ops;
extern struct procfs_entry_ops current_time_ops;

int sys_time(struct user_buffer t);
int sys_gettimeofday(struct user_buffer tv, struct user_buffer tz);
int sys_usleep(useconds_t useconds);

#endif
