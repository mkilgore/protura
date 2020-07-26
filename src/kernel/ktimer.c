/*
 * Copyright (C) 2017 Matt Kilgore
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
#include <protura/fs/procfs.h>
#include <protura/time.h>
#include <arch/drivers/pic8259_timer.h>
#include <protura/ktimer.h>

/*
 * ktimer - Kernel timers
 *
 * Implemented as a sorted list of lists. The main list holds a sorted list of
 * timers, where the one soonest to go off is first.
 *
 * Each timer in that list holds its own list of timers who share the same
 * wake-up tick (if there are any).
 *
 * The above setup allows for very fast handling of timers each tick. Only the
 * first timer has to ever be checked, even if there are multiple timers
 * registered for that tick.
 */

static spinlock_t timers_lock;
static list_head_t timer_list = LIST_HEAD_INIT(timer_list);

/* FIXME: This should be per-CPU, since they can all trigger ktimers.
 *
 * NOTE: This *CANNOT* be used to read the state of the current timer, once we
 * drop the lock in timer_handle_timers() this thing could be free'd at any time.
 *
 * What we can do, however, is do a direct pointer comparison against a
 * pointer provided to us in `timer_cancel()`. If they're equal, the timer is
 * potentially still running, and we need to wait until they don't equal. */
static struct ktimer *currently_running_timer;

void timer_handle_timers(uint64_t tick)
{
    struct ktimer *timer;
    void (*callback) (struct ktimer *);

    while (1) {
        using_spinlock(&timers_lock) {
            currently_running_timer = NULL;

            if (list_empty(&timer_list))
                return;

            timer = list_first_entry(&timer_list, struct ktimer, timer_entry);

            if (timer->wake_up_tick > tick)
                return;

            list_del(&timer->timer_entry);

            /* Store callback because timer might be modified once we release the lock */
            callback = timer->callback;

            currently_running_timer = timer;
        }

        (callback) (timer);
    }
}

int timer_add(struct ktimer *timer, uint64_t ms)
{
    struct ktimer *t;
    timer->wake_up_tick = timer_get_ticks() + ms * (TIMER_TICKS_PER_SEC / 1000);

    using_spinlock(&timers_lock) {
        /* We're already scheduled, don't do anything */
        if (list_node_is_in_list(&timer->timer_entry))
            return -1;

        list_foreach_entry(&timer_list, t, timer_entry) {
            if (t->wake_up_tick >= timer->wake_up_tick) {
                list_add_before(&t->timer_entry, &timer->timer_entry);
                break;
            }
        }

        if (!list_node_is_in_list(&timer->timer_entry))
            list_add_tail(&timer_list, &timer->timer_entry);

        return 0;
    }
}

int timer_del(struct ktimer *timer)
{
    using_spinlock(&timers_lock) {
        if (!list_node_is_in_list(&timer->timer_entry))
            return -1;

        list_del(&timer->timer_entry);
    }

    return 0;
}

void timer_cancel(struct ktimer *timer)
{
    int ret = timer_del(timer);

    /* The easy case, timer wasn't yet run, just return */
    if (!ret)
        return;

    /* Annoying case, timer might currently be running, keep checking
     * `currently_running_timer and yielding. Timers are *supposed* to finish
     * quickly, so this shouldn't last that long. */
    while (1) {
        using_spinlock(&timers_lock) {
            if (currently_running_timer != timer)
                return;
        }

        scheduler_task_yield();
    }
}
