/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/snprintf.h>
#include <protura/semaphore.h>
#include <protura/scheduler.h>

static void __sem_down(struct semaphore *sem)
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

static int __sem_try_down(struct semaphore *sem)
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

static void __sem_up(struct semaphore *sem)
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

void sem_down(struct semaphore *sem)
{
    using_spinlock(&sem->lock)
        __sem_down(sem);
}

int sem_try_down(struct semaphore *sem)
{
    int ret;

    using_spinlock(&sem->lock)
        ret = __sem_try_down(sem);

    return ret;
}

void sem_up(struct semaphore *sem)
{
    using_spinlock(&sem->lock)
        __sem_up(sem);
}

int sem_waiting(struct semaphore *sem)
{
    int c;

    using_spinlock(&sem->lock)
        c = sem->count;

    return c;
}
