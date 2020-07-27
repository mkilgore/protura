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
#include <protura/list.h>
#include <arch/spinlock.h>
#include <protura/snprintf.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/memlayout.h>
#include <protura/mm/palloc.h>
#include <protura/signal.h>
#include <protura/time.h>

#include <protura/scheduler.h>

struct ktimer_sleeper {
    struct ktimer timer;
    struct task *task;
};

static void sleeper_callback(struct ktimer *timer)
{
    struct ktimer_sleeper *sleeper = container_of(timer, struct ktimer_sleeper, timer);

    if (!sleeper->task)
        return;

    struct task *t = sleeper->task;
    sleeper->task = NULL;
    scheduler_task_wake(t);
}

static int sleeper_inner_ms(int ms, int check_signals)
{
    struct task *current = cpu_get_local()->current;

    struct ktimer_sleeper sleeper = {
        .timer = KTIMER_CALLBACK_INIT(sleeper.timer, sleeper_callback),
        .task = current,
    };

    timer_add(&sleeper.timer, ms);

    while (1) {
        if (check_signals)
            scheduler_set_intr_sleeping();
        else
            scheduler_set_sleeping();

        int sig_pending = check_signals? has_pending_signal(current): 0;
        int timer_fired = timer_was_fired(&sleeper.timer);

        if (sig_pending || timer_fired)
            break;

        scheduler_task_yield();
    }

    scheduler_set_running();

    timer_cancel(&sleeper.timer);

    return sleeper.task? -EINTR: 0;
}

int task_sleep_ms(int ms)
{
    return sleeper_inner_ms(ms, 0);
}

int task_sleep_intr_ms(int ms)
{
    return sleeper_inner_ms(ms, 1);
}

int sys_sleep(int seconds)
{
    return task_sleep_intr_ms(seconds * 1000);
}

int sys_usleep(useconds_t useconds)
{
    /*
     * This rounds up if the value is not an exact millisecond value
     * This is necessary to ensure we sleep for *at least* useconds
     */
    useconds_t ms = (useconds + 999) / 1000;
    return task_sleep_intr_ms(ms);
}
