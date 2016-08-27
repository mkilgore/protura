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

typedef int32_t  time_t;
typedef long     suseconds_t;

/* Number of days from year 0 to the start of the Unix Epoch, 1970-01-01 */
#define TIME_DAYS_TO_EPOCH 719499

struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};

time_t protura_uptime_get(void);
void protura_uptime_inc(void);

 /*
  * Used when we read the RTC time on boot, if we have one. This allowes us to
  * sync the uptime to starting aproximately when the RTC time was.
  */
void protura_uptime_reset(void);

time_t protura_boot_time_get(void);
void protura_boot_time_set(time_t t);

time_t protura_current_time_get(void);

int protura_uptime_read(void *, size_t, size_t *);
int protura_boot_time_read(void *, size_t, size_t *);
int protura_current_time_read(void *, size_t, size_t *);

int sys_time(time_t *t);

#endif
