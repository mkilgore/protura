/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#include <protura/types.h>
#include <protura/debug.h>
#include <protura/atomic.h>
#include <protura/mm/palloc.h>
#include <protura/time.h>

#include <arch/cpu.h>
#include <arch/asm.h>
#include <arch/idt.h>
#include <arch/task.h>
#include <arch/drivers/pic8259.h>
#include <arch/drivers/pic8259_timer.h>

#define RTC_PORT_OUT (0x70)
#define RTC_PORT_IN (RTC_PORT_OUT + 1)

uint8_t rtc_reg_read(int reg)
{
    outb(RTC_PORT_OUT, reg);
    return inb(RTC_PORT_IN);
}

enum {
    RTC_REG_SECONDS = 0x00,
    RTC_REG_MINUTES = 0x02,
    RTC_REG_HOURS = 0x04,
    RTC_REG_WEEKDAY = 0x06,
    RTC_REG_DAY_OF_MONTH = 0x07,
    RTC_REG_MONTH = 0x08,
    RTC_REG_YEAR = 0x09,
    RTC_REG_STATUS_A = 0x0A,
    RTC_REG_STATUS_B = 0x0B,
};

enum {
    RTC_STATUS_A_UIP = 0x80, /* Update in progress */
};

enum {
    RTC_STATUS_B_IS_BINARY = 0x04, /* Bit indicates binary values - else BCD values are given */
};

static int bcd_to_bin(int bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static void rtc_get_time_full(int *sec, int *min, int *hour, int *day, int *mon, int *year)
{
    /*
     * Extremely similar to the Linux Kernel's technique.
     *
     * We read the time immdieatly after a new update has happened - this
     * ensures we have enough time to read the values before another update.
     *
     * This requires first waiting for an update, and then waiting for that
     * update to end. The worst-case is that this take a second.
     */
    while (!(rtc_reg_read(RTC_REG_STATUS_A) & RTC_STATUS_A_UIP))
        ;
    while (rtc_reg_read(RTC_REG_STATUS_A) & RTC_STATUS_A_UIP)
        ;

    *sec= rtc_reg_read(RTC_REG_SECONDS);
    *min = rtc_reg_read(RTC_REG_MINUTES);
    *hour = rtc_reg_read(RTC_REG_HOURS);
    *day = rtc_reg_read(RTC_REG_DAY_OF_MONTH);
    *mon = rtc_reg_read(RTC_REG_MONTH);
    *year = rtc_reg_read(RTC_REG_YEAR);

    if (!(rtc_reg_read(RTC_REG_STATUS_B) & RTC_STATUS_B_IS_BINARY)) {
        *sec = bcd_to_bin(*sec);
        *min = bcd_to_bin(*min);
        *hour = bcd_to_bin(*hour);
        *day = bcd_to_bin(*day);
        *mon = bcd_to_bin(*mon);
        *year = bcd_to_bin(*year);
    }

    /* Adjust for the fact that the RTC reports '2016' as 16, and '1994' as '94' */
    *year += 1900;
    if (*year < 1960)
        *year += 100;

    kp(KP_TRACE, "Year: %d, Mon: %d, Day: %d, Hour: %d, Min: %d, Sec: %d\n",
            *year, *mon, *day, *hour, *min, *sec);
}

static time_t mktime(int sec, int min, int hour, int day, int mon, int year)
{
    time_t base;
    /* Black magic */

    /*
     * This modifies the order of the months, from 1 to 12, to 3 to 12, 1, 2.
     * This is important as it places Feb last - necessary for a later calculation.
     *
     * When the number of months drops to zero or below, we add the number of
     * months from the previous year and subtract 1 from the actual year.
     * Again, for a later calculation.
     */
    mon -= 2;
    if (mon <= 0) {
        mon += 12;
        year -= 1;
    }

    /* First, calculate number of leap days */
    base = (year / 4 - year / 100 + year / 400);

    /* Calculate the number of days till the current month of the year
     *
     * Uses the fiddling done above. This calculation generates the 30/31
     * sequence needed for counting the days in the months (Starting with
     * March=1). */
    base += 367 * mon / 12;

    /* Add days into the month */
    base += day;

    /* Add the numbrer of days in the rest of the years */
    base += year * 365;

    /* Base the number of days off of the Unix epoch */
    base -= TIME_DAYS_TO_EPOCH;

    /* Calculate hours */
    base = base * 24 + hour;

    /* Calculate minutes */
    base = base * 60 + min;

    /* Calculate seconds */
    base = base * 60 + sec;

    return base;
}

void rtc_update_time(void)
{
    int sec, min, hour, day, mon, year;

    rtc_get_time_full(&sec, &min, &hour, &day, &mon, &year);
    protura_uptime_reset();
    protura_boot_time_set(mktime(sec, min, hour, day, mon, year));
}

