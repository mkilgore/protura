/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_MUTEX_H
#define INCLUDE_PROTURA_MUTEX_H

#include <protura/debug.h>
#include <protura/semaphore.h>

typedef struct semaphore mutex_t;

#define MUTEX_INIT(sem, name) SEM_INIT(sem, name, 1)

static inline void mutex_init(mutex_t *mut)
{
    sem_init(mut, 1);
}

static inline void mutex_lock(mutex_t *mut)
{
    kp(KP_LOCK, "Mutex %p: Locking\n", mut);
    sem_down(mut);
    kp(KP_LOCK, "Mutex %p: Locked\n", mut);
}

static inline int mutex_try_lock(mutex_t *mut)
{
    kp(KP_LOCK, "Mutex %p: Attempting Locking\n", mut);
    if (sem_try_down(mut)) {
        kp(KP_LOCK, "Mutex %p: Locked\n", mut);
        return 1;
    }
    return 0;
}

static inline void mutex_unlock(mutex_t *mut)
{
    kp(KP_LOCK, "Mutex %p: Unlocking\n", mut);
    sem_up(mut);
    kp(KP_LOCK, "Mutex %p: Unlocked\n", mut);
}

static inline int mutex_waiting(mutex_t *mut)
{
    return sem_waiting(mut);
}

static inline void mutex_lock_cleanup(mutex_t **mut)
{
    mutex_lock(*mut);
}

static inline void mutex_unlock_cleanup(mutex_t **mut)
{
    mutex_unlock(*mut);
}

#define scoped_mutex(mut) \
    scoped_using_cond(1, mutex_lock, mutex_unlock_cleanup, mut)

#define not_scoped_mutex(mut) \
    scoped_using_cond(1, mutex_unlock, mutex_lock_cleanup, mut)

#define using_mutex(mut)     scoped_mutex(mut)
#define not_using_mutex(mut) not_scoped_mutex(mut)

#endif
