/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_SPINLOCK_H
#define INCLUDE_ARCH_SPINLOCK_H

#include <protura/stddef.h>

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

void spinlock_acquire(spinlock_t *);
void spinlock_release(spinlock_t *);

int spinlock_try_acquire(spinlock_t *);

/* Wraps acquiring and releaseing a spinlock. Usages of 'using_spinlock' can't
 * ever leave-out a matching release for the acquire. */
#define using_spinlock(lock) using_nocheck(spinlock_acquire(lock), spinlock_release(lock))

/* Can be used in a 'using_spinlock' block of code to release a lock for a
 * section of code, and then acquire it back after that code is done.
 *
 * It's useful in sections of code where we may go to sleep, and we want to
 * release the lock before yielding, and then acquire the lock back when we
 * start running again. */
#define not_using_spinlock(lock) using_nocheck(spinlock_release(lock), spinlock_acquire(lock))

#endif
