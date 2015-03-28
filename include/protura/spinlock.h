#ifndef INCLUDE_PROTURA_SPINLOCK_H
#define INCLUDE_PROTURA_SPINLOCK_H

#include <protura/stddef.h>

struct spinlock {
    unsigned int locked;
    unsigned int eflags;
    char *name;
};

#define SPINLOCK_INIT(nam) { .locked = 0, .name = nam }

void spinlock_acquire(struct spinlock *);
void spinlock_release(struct spinlock *);

/* Wraps acquiring and releaseing a spinlock. Usages of 'using_spinlock' can't
 * ever leave-out a matching release for the acquire. */
#define using_spinlock(lock) using_nocheck(spinlock_acquire(lock), spinlock_release(lock))

#endif
