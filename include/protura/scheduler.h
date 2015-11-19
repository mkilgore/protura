#ifndef INCLUDE_ARCH_SCHEDULER_H
#define INCLUDE_ARCH_SCHEDULER_H

#include <protura/stddef.h>
#include <protura/task.h>
#include <protura/list.h>
#include <protura/queue.h>
#include <arch/cpu.h>

pid_t scheduler_next_pid(void);

void scheduler_task_add(struct task *);
void scheduler_task_remove(struct task *);

void scheduler_task_yield(void);
void scheduler_task_yield_preempt();

/* Called after task is completely read to be removed - after sys_exit, and
 * sys_wait */
void scheduler_task_dead(void);
void scheduler_task_mark_dead(struct task *t);

static inline enum task_state scheduler_task_get_state(struct task *t)
{
    return t->state;
}

static inline void scheduler_task_set_state(struct task *t, enum task_state state)
{
    t->state = state;
}

static inline void scheduler_set_state(enum task_state state)
{
    scheduler_task_set_state(cpu_get_local()->current, state);
}

#define scheduler_task_set_sleeping(t) scheduler_task_set_state(t, TASK_SLEEPING)
#define scheduler_task_set_runnable(t) scheduler_task_set_state(t, TASK_RUNNABLE)

/* These macros are used to set the current task's state */
#define scheduler_set_sleeping() scheduler_set_state(TASK_SLEEPING)
#define scheduler_set_running()  scheduler_set_state(TASK_RUNNING)

static inline void scheduler_task_wake(struct task *t)
{
    if (t->state == TASK_SLEEPING)
        t->state = TASK_RUNNABLE;
}


void scheduler_task_waitms(uint32_t mseconds);
void sys_sleep(uint32_t mseconds);
void scheduler(void);

/* Entry point for all new tasks */
void scheduler_task_entry(void);

/* This macro defines the 'sleep' block. It is used to wrap a section of code
 * in 'sleeping' and 'running' task states, where any yields inside of the
 * 'sleep' block will result in the current task sleeping instead of yielding.
 *
 * The reason this is used instead of a simple 'scheduler_task_sleep()'
 * function is to avoid 'lost-wakeups', which can happen if you don't set your
 * state to SLEEPING before checking if you should sleep, and then set it to
 * RUNNING afterward. For example, if the keyboard is going to wake you up on
 * events, and you want to sleep until the keyboard has data to read, it would
 * be used like this:
 *
 * sleep {
 *     if (!keyboard_has_char())
 *         scheduler_task_yield();
 * }
 *
 * Which translates into this:
 *
 * scheduler_set_sleeping();
 *     if (!keyboard_has_char())
 *         scheduler_task_yield();
 * scheduler_set_running();
 *
 * Which saves you from losing wakeups from the keyboard if data comes between
 * the check of 'keyboard_has_char()' and your yield. This is because you set
 * your state to SLEEPING *before* your check, and the wakeup, if it happens,
 * will revert your state back to RUNNING. Thus, when you yield, either your
 * state is SLEEPING and you didn't miss a wakeup, or your state is RUNNING and
 * you did but you won't sleep, so there is no problem. 
 *
 *
 * 'sleep_with_wait_queue' in wait.h is almost identical to 'sleep', but when
 * you yield inside of a 'sleep_on_wait_queue' block, you sleep until you're
 * woke-up from the wait_queue that you passed to 'sleep_with_wait_queue' (Or
 * something else wakes you up).
 *
 * It's worth pointing out that on first glance 'sleep_with_wait_queue' appears
 * to have a small possible error, in that if you have to loop and possibly
 * sleep multiple times, you'll register with the wait_queue multiple times.
 * This is not an error, however, because if this happens you actually *do*
 * need to register multiple times. When you wake you are removed from the
 * queue, and you have to register yourself *again* if you need to woke-up a
 * second time.
 *
 * NOTE: It's important to understand that these blocks are *not* loops. 
 */
#define sleep using_nocheck(scheduler_set_sleeping(), scheduler_set_running())


#endif
