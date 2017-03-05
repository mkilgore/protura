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
    list_head_t queue;
};

struct semaphore_wait_entry {
    list_node_t next;
    struct task *task;
};

typedef struct semaphore semaphore_t;

#define SEM_INIT(sem, name, cnt) \
    { .lock = SPINLOCK_INIT(name), \
      .count = (cnt), \
      .queue = LIST_HEAD_INIT((sem).queue) }

static inline void sem_init(struct semaphore *sem, int value)
{
    memset(sem, 0, sizeof(*sem));

    spinlock_init(&sem->lock, "Sem lock");
    sem->count = value;
    list_head_init(&sem->queue);
}

static inline void sem_wait_entry_init(struct semaphore_wait_entry *ent)
{
    list_node_init(&ent->next);
    ent->task = NULL;
}

static inline void __sem_down(struct semaphore *sem)
{
    struct semaphore_wait_entry ent;
    kp(KP_LOCK, "semaphore %p: down: %d\n", sem, sem->count);

    sem_wait_entry_init(&ent);
    ent.task = cpu_get_local()->current;

  sleep_again:
    if (!list_node_is_in_list(&ent.next))
        list_add_tail(&sem->queue, &ent.next);

    sleep {
        if (sem->count <= 0) {
            not_using_spinlock(&sem->lock)
                scheduler_task_yield();

            goto sleep_again;
        }
    }
    sem->count--;
    list_del(&ent.next);

    kp(KP_LOCK, "semaphore %p down result: %d\n", sem, sem->count);
}

static inline int __sem_try_down(struct semaphore *sem)
{
    kp(KP_LOCK, "semaphore %p: Trying down: %d\n", sem, sem->count);
    /* We have the lock on the semaphore, so it's guarenteed sem->count won't
     * change in this context. */
    if (sem->count <= 0)
        return 0;

    /* Since we hold the lock, nobody can change sem->count, so we're fine to
     * decrement it and not check it. */
    sem->count--;

    kp(KP_LOCK, "semaphore %p: down result: %d\n", sem, sem->count);

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
    struct semaphore_wait_entry *ent;
    int wake = 0;
    sem->count++;

    if (!list_empty(&sem->queue)) {
        ent = list_take_first(&sem->queue, struct semaphore_wait_entry, next);
        scheduler_task_wake(ent->task);
        wake = 1;
    }

    kp(KP_LOCK, "semaphore %p: up: %d, %d\n", sem, sem->count, wake);
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
