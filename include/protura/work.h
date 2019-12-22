/*
 * Copyright (C) 2017 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_WORK_H
#define INCLUDE_PROTURA_WORK_H

#include <protura/types.h>
#include <protura/list.h>
#include <protura/bits.h>
#include <protura/ktimer.h>
#include <protura/spinlock.h>

struct task;

#define WORKQUEUE_MAX_THREADS 5

/*
 * Holds one or more threads that `struct work` entries will be scheduled on.
 */
struct workqueue {
    list_head_t work_list;
    list_head_t work_running_list;
    spinlock_t lock;
    struct task *work_threads[WORKQUEUE_MAX_THREADS];
    int thread_count;
    int wake_next_thread;
};

/*
 * `struct work` is the kernel primitive for triggering some type of "work" to be
 * done due to some event. The person registering the "work" is the one who
 * sets up what happens on the event, meaning anything that uses a `struct
 * work` can trigger a variety of events, which are needed by various parts of
 * the kernel, making it very flexable.
 *
 * Current types of work:
 *
 *     WORK_TASK: Wake up the task specified by the `task` field.
 *
 *     WORK_CALLBACK: Call the callback specified by the `callback` field.
 *                    The `struct work` instance is passed to this callback,
 *                    with the assumption being it is embeeded in some larger
 *                    structure that will be accessed via `container_of`.
 *
 *     WORK_KWORK: Schedules this work to run on the `kwork` workqueue.
 *
 *     WORK_WORKQUEUE: Schedules this work to run on the workqueue from the
 *                     `queue` field.
 *
 *     WORK_NONE: Do nothing
 *
 *
 * Each of those types have their own `_INIT` macros and `_init` functions for
 * filling them in. After a `struct work` is initialized, it can be scheduled
 * by passing it to `work_schedule`.
 */

enum work_type {
    WORK_NONE,
    WORK_TASK,
    WORK_CALLBACK,
    WORK_KWORK,
    WORK_WORKQUEUE,
};

enum work_flags {
    WORK_SCHEDULED,
    WORK_ONESHOT,
};

struct work {
    /* Entry into the queue of work to be run */
    list_node_t work_entry;

    /* Entry in a wakeup queue */
    list_node_t wakeup_entry;

    flags_t flags;

    enum work_type type;

    void (*callback) (struct work *);
    struct task *task;
    struct workqueue *queue;
};

struct delay_work {
    struct work work;
    struct ktimer timer;
};

#define WORKQUEUE_INIT(queue) \
    { \
        .work_list = LIST_HEAD_INIT((queue).work_list), \
        .work_running_list = LIST_HEAD_INIT((queue).work_running_list), \
        .lock = SPINLOCK_INIT(), \
    }

#define WORK_INIT(work) \
    { \
        .work_entry = LIST_NODE_INIT((work).work_entry), \
        .wakeup_entry = LIST_NODE_INIT((work).wakeup_entry), \
        .type = WORK_NONE, \
    }

#define WORK_INIT_KWORK(work, func) \
    { \
        .work_entry = LIST_NODE_INIT((work).work_entry), \
        .wakeup_entry = LIST_NODE_INIT((work).wakeup_entry), \
        .type = WORK_KWORK, \
        .callback = func, \
    }

#define WORK_INIT_CALLBACK(work, func) \
    { \
        .work_entry = LIST_NODE_INIT((work).work_entry), \
        .wakeup_entry = LIST_NODE_INIT((work).wakeup_entry), \
        .type = WORK_CALLBACK, \
        .callback = func, \
    }

#define WORK_INIT_TASK(work, t) \
    { \
        .work_entry = LIST_NODE_INIT((work).work_entry), \
        .wakeup_entry = LIST_NODE_INIT((work).wakeup_entry), \
        .type = WORK_TASK, \
        .task = t, \
    }

#define WORK_INIT_WORKQUEUE(work, func, q) \
    { \
        .work_entry = LIST_NODE_INIT((work).work_entry), \
        .wakeup_entry = LIST_NODE_INIT((work).wakeup_entry), \
        .type = WORK_WORKQUEUE, \
        .callback = func, \
        .queue = (q), \
    }

#define DELAY_WORK_INIT(w) \
    { \
        .work = WORK_INIT((w).work), \
        .timer = KTIMER_INIT((w).timer), \
    }

#define DELAY_WORK_INIT_KWORK(w, func) \
    { \
        .work = WORK_INIT_KWORK((w).work, func), \
        .timer = KTIMER_INIT((w).timer), \
    }

static inline void workqueue_init(struct workqueue *queue)
{
    *queue = (struct workqueue)WORKQUEUE_INIT(*queue);
}

static inline void work_init(struct work *work)
{
    *work = (struct work)WORK_INIT(*work);
}

static inline void work_init_kwork(struct work *work, void (*callback) (struct work *))
{
    *work = (struct work)WORK_INIT_KWORK(*work, callback);
}

static inline void work_init_callback(struct work *work, void (*callback) (struct work *))
{
    *work = (struct work)WORK_INIT_CALLBACK(*work, callback);
}

static inline void work_init_task(struct work *work, struct task *task)
{
    *work = (struct work)WORK_INIT_TASK(*work, task);
}

static inline void work_init_workqueue(struct work *work, void (*callback) (struct work *), struct workqueue *queue)
{
    *work = (struct work)WORK_INIT_WORKQUEUE(*work, callback, queue);
}

static inline void delay_work_init_kwork(struct delay_work *work, void (*callback) (struct work *))
{
    *work = (struct delay_work)DELAY_WORK_INIT_KWORK(*work, callback);
}

void workqueue_start(struct workqueue *, const char *thread_name);
void workqueue_start_multiple(struct workqueue *, const char *thread_name, int thread_count);
void workqueue_stop(struct workqueue *);

void workqueue_add_work(struct workqueue *, struct work *);

void work_schedule(struct work *);

/* Returns 0 if work was newly scheduled, -1 if work was already scheduled */
int kwork_delay_schedule(struct delay_work *, int delay_ms);

/* Returns 0 if the timer was deleted, -1 if the timer was not scheduled */
int kwork_delay_unschedule(struct delay_work *);

/*
 * Convinence methods for setting the callback and scheduling in one go.
 */
static inline void kwork_schedule_callback(struct work *work, void (*callback) (struct work *))
{
    work->callback = callback;
    work->type = WORK_KWORK;
    work_schedule(work);
}

static inline void kwork_delay_schedule_callback(struct delay_work *work, int delay_ms, void (*callback) (struct work *))
{
    work->work.callback = callback;
    kwork_delay_schedule(work, delay_ms);
}

void kwork_init(void);

#endif
