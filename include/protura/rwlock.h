/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_RWLOCK_H
#define INCLUDE_PROTURA_RWLOCK_H

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/spinlock.h>
#include <protura/scheduler.h>
#include <protura/wait.h>

/* If count is '-1', that indicates a writer currently holds the lock.  A
 * postiive count is the current number of readers */
struct rwlock {
    spinlock_t lock;

    int count;
    struct wait_queue readers;
    struct wait_queue writers;
};

typedef struct rwlock rwlock_t;

#define RWLOCK_INIT(rwlock, name) \
    { .lock = SPINLOCK_INIT(name), \
      .count = 0, \
      .readers = WAIT_QUEUE_INIT((rwlock).readers, "RWLOCK Readers queue"), \
      .writers = WAIT_QUEUE_INIT((rwlock).writers, "RWLOCK Writers queue") }

static inline void rwlock_init(rwlock_t *rwlock)
{
    memset(rwlock, 0, sizeof(*rwlock));

    spinlock_init(&rwlock->lock, "RWLock spinlock");
    rwlock->count = 0;
    wait_queue_init(&rwlock->writers);
    wait_queue_init(&rwlock->readers);
}

static inline void __rwlock_rlock(rwlock_t *rwlock)
{
  sleep_again:
    sleep_with_wait_queue(&rwlock->readers) {
        if (rwlock->count < 0) {
            not_using_spinlock(&rwlock->lock)
                scheduler_task_yield();

            goto sleep_again;
        }
    }

    rwlock->count++;
}

static inline void rwlock_rlock(rwlock_t *rwlock)
{
    using_spinlock(&rwlock->lock)
        __rwlock_rlock(rwlock);
}

static inline void __rwlock_runlock(rwlock_t *rwlock)
{
    rwlock->count--;

    /* Hitting zero is special. It indicates no readers, so no need to wake the
     * readers queue, and also that a writer could take the lock, so we need to
     * wake-up a potential writer. */
    if (rwlock->count == 0)
        wait_queue_wake(&rwlock->writers);
    else
        wait_queue_wake_all(&rwlock->readers);
}

static inline void rwlock_runlock(rwlock_t *rwlock)
{
    using_spinlock(&rwlock->lock)
        __rwlock_runlock(rwlock);
}

static inline void __rwlock_wlock(rwlock_t *rwlock)
{
  sleep_again:
    sleep_with_wait_queue(&rwlock->writers) {
        if (rwlock->count != 0) {
            not_using_spinlock(&rwlock->lock)
                scheduler_task_yield();

            goto sleep_again;
        }
    }

    rwlock->count--;
}

static inline void rwlock_wlock(rwlock_t *rwlock)
{
    using_spinlock(&rwlock->lock)
        __rwlock_wlock(rwlock);
}

static inline void __rwlock_wunlock(rwlock_t *rwlock)
{
    rwlock->count = 0;

    /* Since we just did a write, we're better off waking any writers if we
     * have them, since we know nobody else is currently using this lock and
     * it's a good opportunity for a writer.
     *
     * If we have no current writers sleeping in the queue, then we wake all
     * the readers. */
    if (wait_queue_wake(&rwlock->writers) == 0)
        wait_queue_wake_all(&rwlock->readers);
}

static inline void rwlock_wunlock(rwlock_t *rwlock)
{
    using_spinlock(&rwlock->lock)
        __rwlock_wunlock(rwlock);
}

#define using_rwlock_r(rwlock) \
    using_nocheck(rwlock_rlock(rwlock), rwlock_runlock(rwlock))

#define using_rwlock_w(rwlock) \
    using_nocheck(rwlock_wlock(rwlock), rwlock_wunlock(rwlock))

#endif
