#ifndef INCLUDE_PROTURA_KTIMER_H
#define INCLUDE_PROTURA_KTIMER_H

#include <protura/types.h>
#include <protura/list.h>
#include <protura/bits.h>

/*
 * ktimer - Kernel timers
 *
 * Timers that can be set to trigger a callback (in an interrupt context) after
 * a certain number of milliseconds have gone by.
 */

enum ktimer_flags {
    KTIMER_FIRED,
};

struct ktimer {
    list_node_t timer_entry;
    list_head_t timer_list;

    flags_t flags;

    uint64_t wake_up_tick;

    void (*callback) (struct ktimer *);
};

#define KTIMER_INIT(timer) \
    { \
        .timer_entry = LIST_NODE_INIT((timer).timer_entry), \
        .timer_list = LIST_HEAD_INIT((timer).timer_list), \
    }

static inline void ktimer_init(struct ktimer *timer)
{
    *timer = (struct ktimer)KTIMER_INIT(*timer);
}

static inline void ktimer_clear(struct ktimer *timer)
{

}

void timer_handle_timers(uint64_t tick);
void timer_add(struct ktimer *timer, uint64_t ms);
int timer_del(struct ktimer *timer);

#endif
