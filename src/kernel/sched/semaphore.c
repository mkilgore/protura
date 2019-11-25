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

static void __sem_wake_if_waiting(struct semaphore *sem)
{
    struct semaphore_wait_entry *ent;

    if (!list_empty(&sem->queue)) {
        ent = list_first_entry(&sem->queue, struct semaphore_wait_entry, next);
        scheduler_task_wake(ent->task);
    }
}

static void __sem_down(struct semaphore *sem)
{
    struct semaphore_wait_entry ent;
    kp(KP_LOCK, "semaphore %p: down: %d\n", sem, sem->count);

    sem_wait_entry_init(&ent);
    ent.task = cpu_get_local()->current;

    /*
     * The logic here is actually fairly straight forward:
     *
     * 1. You add yourself at the end of the queue of tasks waiting on this semaphore
     * 2. You wait until sem->count > 0 (sleeping and releasing sem->lock if necessary)
     * 3. You remove yourself from the list and decrement sem->count
     * 4. We wake the next task if the semaphore still has space.
     *
     * The 4th step is probably the most interesting. sem_up never modifies the
     * sem->queue, so if sem_up is done twice before the first sleeping task
     * wakes up, then that task receives two wake-ups. This we may exit the
     * sleeping with sem->count > 1. When that happens we have to make sure we
     * wake up the next waiter (if there is one) to prevent a stall where
     * waiters wouldn't be woken-up immediately.
     */
    list_add_tail(&sem->queue, &ent.next);

    sleep_event_spinlock(sem->count > 0, &sem->lock);

    list_del(&ent.next);
    sem->count--;

    if (sem->count > 0)
        __sem_wake_if_waiting(sem);

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
    sem->count++;
    __sem_wake_if_waiting(sem);
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
