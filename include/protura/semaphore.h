/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_SEMAPHORE_H
#define INCLUDE_PROTURA_SEMAPHORE_H

#include <protura/debug.h>
#include <protura/string.h>
#include <arch/spinlock.h>
#include <protura/scheduler.h>
#include <protura/wait.h>

struct semaphore {
    spinlock_t lock;

    int count;
    struct wait_queue queue;
};

typedef struct semaphore semaphore_t;

#define SEM_INIT(sem, name, cnt) \
    { .lock = SPINLOCK_INIT(name), \
      .count = (cnt), \
      .queue = WAIT_QUEUE_INIT((sem).queue, "SEM wait queue") }

static inline void sem_init(struct semaphore *sem, int value)
{
    memset(sem, 0, sizeof(*sem));

    spinlock_init(&sem->lock, "Sem lock");
    sem->count = value;
    wait_queue_init(&sem->queue);
}

static inline void __sem_down(struct semaphore *sem)
{
    sem->count--;

  sleep_again:
    sleep_with_wait_queue(&sem->queue) {
        if (sem->count < 0) {
            not_using_spinlock(&sem->lock)
                scheduler_task_yield();

            goto sleep_again;
        }
    }
}

static inline int __sem_try_down(struct semaphore *sem)
{
    kprintf("Sem trydown: %d\n", sem->count);
    /* We have the lock on the semaphore, so it's guarenteed sem->count won't
     * change in this context. */
    if (sem->count <= 0)
        return 0;

    /* Since we hold the lock, nobody can change sem->count, so we're fine to
     * decrement it and not check it. */
    sem->count--;

    return 1;
}

static inline void sem_down(struct semaphore *sem)
{
    using_spinlock(&sem->lock)
        __sem_down(sem);
}

static inline int sem_try_down(struct semaphore *sem)
{
    int ret;

    using_spinlock(&sem->lock)
        ret = __sem_try_down(sem);

    return ret;
}

static inline void __sem_up(struct semaphore *sem)
{
    sem->count++;

    if (sem->count <= 0)
        wait_queue_wake(&sem->queue);
}

static inline void sem_up(struct semaphore *sem)
{
    using_spinlock(&sem->lock)
        __sem_up(sem);
}

static inline int sem_waiting(struct semaphore *sem)
{
    int c;

    using_spinlock(&sem->lock)
        c = sem->count;

    return c;
}

#endif
