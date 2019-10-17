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
#include <protura/mm/user_ptr.h>
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

void timer_handle_timers(uint64_t tick)
{
    struct ktimer *timer, *next;

    while (1) {
        scoped_spinlock(&timers_lock) {
            timer = list_first_entry(&timer_list, struct ktimer, timer_entry);

            if (timer->wake_up_tick != tick)
                return;

            list_del(&timer->timer_entry);
        }

        list_head_t list = LIST_HEAD_INIT(list);
        list_replace_init(&list, &timer->timer_list);

        /* We're careful to release the spinlock before calling the callbacks */
        (timer->callback) (timer);

        list_foreach_take_entry(&list, next, timer_entry)
            (next->callback) (next);

    }
}

void timer_add(struct ktimer *timer, uint64_t ms)
{
    struct ktimer *t;
    timer->wake_up_tick = timer_get_ticks() + ms * (TIMER_TICKS_PER_SEC / 1000);

    using_spinlock(&timers_lock) {
        list_foreach_entry(&timer_list, t, timer_entry) {
            if (t->wake_up_tick == timer->wake_up_tick)
                list_add_tail(&t->timer_list, &timer->timer_entry);
            else if (t->wake_up_tick > timer->wake_up_tick)
                list_add_tail(&t->timer_entry, &timer->timer_entry);
        }

        if (list_ptr_is_head(&t->timer_entry, &timer_list))
            list_add_tail(&timer_list, &timer->timer_entry);
    }
}

int timer_del(struct ktimer *timer)
{
    using_spinlock(&timers_lock) {
        if (!list_node_is_in_list(&timer->timer_entry))
            return -1;

        if (list_empty(&timer->timer_list)) {
            list_del(&timer->timer_entry);
        } else {
            struct ktimer *next = list_first_entry(&timer->timer_list, struct ktimer, timer_entry);

            /*
             * Takes this setup:
             *
             * timer_list
             *     |
             *     V
             *   timer -> next -> ...
             *     |
             *     V
             *   ...
             *
             * And turns it into this:
             *
             * timer_list
             *     |
             *     V
             *   next -> ...
             *     |
             *     V
             *   ...
             *
             * It makes 'next' the head of the list that used to extend from
             * 'timer' (that 'next' was a part of) and also puts 'next' on the
             * list extending from 'timer_list'
             */

            list_del(&next->timer_entry);
            list_replace(&timer->timer_list, &next->timer_list);
            list_replace(&timer->timer_entry, &next->timer_entry);
        }
    }

    return 0;
}

