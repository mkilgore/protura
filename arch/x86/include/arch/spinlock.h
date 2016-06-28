/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_SPINLOCK_H
#define INCLUDE_ARCH_SPINLOCK_H

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/stddef.h>
#include <arch/asm.h>

struct spinlock {
    unsigned int locked;
    unsigned int eflags;
    const char *name;
};

typedef struct spinlock spinlock_t;

#define SPINLOCK_INIT(nam) { .locked = 0, .name = nam }

static inline void spinlock_init(spinlock_t *lock, const char *name)
{
    *lock = (spinlock_t)SPINLOCK_INIT(name);
}

/* The 'nolog' versions perform exactly the same as the regular versions,
 * however the are guarenteed to not call 'kp()'. This is only really important
 * for the code inside of 'kp()' which may have to take spinlocks to guarentee
 * output consistency. */
static inline void spinlock_acquire_nolog(spinlock_t *lock)
{
    lock->eflags = eflags_read();
    cli();
    while (xchg(&lock->locked, 1) != 0)
        ;
}

static inline void spinlock_release_nolog(spinlock_t *lock)
{
    xchg(&lock->locked, 0);
    eflags_write(lock->eflags);
}

static inline void spinlock_acquire(spinlock_t *lock)
{
    kp(KP_SPINLOCK, "Spinlock %s:%p: Locking\n", lock->name, lock);
    spinlock_acquire_nolog(lock);
    kp(KP_SPINLOCK, "Spinlock %s:%p: Locked\n", lock->name, lock);
}

static inline void spinlock_release(spinlock_t *lock)
{
    kp(KP_SPINLOCK, "Spinlock %s:%p: Unlocking\n", lock->name, lock);
    spinlock_release_nolog(lock);
    kp(KP_SPINLOCK, "Spinlock %s:%p: Unlocked\n", lock->name, lock);
}

static inline void spinlock_release_cleanup(spinlock_t **spinlock)
{
    spinlock_release(*spinlock);
}

static inline void spinlock_release_nolog_cleanup(spinlock_t **spinlock)
{
    spinlock_release_nolog(*spinlock);
}

static inline int spinlock_try_acquire(spinlock_t *lock)
{
    uint32_t eflags;
    int got_lock;

    kp(KP_SPINLOCK, "Spinlock %s:%p: Trying locking\n", lock->name, lock);

    eflags = eflags_read();
    cli();

    got_lock = xchg(&lock->locked, 1);

    if (got_lock == 0) {
        lock->eflags = eflags;
        kp(KP_SPINLOCK, "Spinlock %s:%p: Locked\n", lock->name, lock);
    } else {
        eflags_write(eflags);
        kp(KP_SPINLOCK, "Spinlock %s:%p: Not locked\n", lock->name, lock);
    }

    return got_lock == 0;
}

/* Wraps acquiring and releaseing a spinlock. Usages of 'using_spinlock' can't
 * ever leave-out a matching release for the acquire. */
#define using_spinlock(lock) using_nocheck(spinlock_acquire(lock), spinlock_release(lock))
#define using_spinlock_nolog(lock) using_nocheck(spinlock_acquire_nolog(lock), spinlock_release_nolog(lock))

#define scoped_spinlock(lock) scoped_using_cond(1, spinlock_acquire, spinlock_release_cleanup, lock)
#define scoped_spinlock_nolog(lock) scoped_using_cond(1, spinlock_acquire, spinlock_release_nolog_cleanup, lock)

/* Can be used in a 'using_spinlock' block of code to release a lock for a
 * section of code, and then acquire it back after that code is done.
 *
 * It's useful in sections of code where we may go to sleep, and we want to
 * release the lock before yielding, and then acquire the lock back when we
 * start running again. */
#define not_using_spinlock(lock) using_nocheck(spinlock_release(lock), spinlock_acquire(lock))

#endif
