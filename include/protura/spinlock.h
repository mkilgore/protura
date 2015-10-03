#ifndef INCLUDE_PROTURA_SPINLOCK_H
#define INCLUDE_PROTURA_SPINLOCK_H

#include <protura/stddef.h>

struct spinlock {
    unsigned int locked;
    unsigned int eflags;
    const char *name;
};

#define SPINLOCK_INIT(nam) { .locked = 0, .name = nam }

static inline void spinlock_init(struct spinlock *lock, const char *name)
{
    lock->locked = 0;
    lock->name = name;
}

void spinlock_acquire(struct spinlock *);
void spinlock_release(struct spinlock *);

int spinlock_try_acquire(struct spinlock *);

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
