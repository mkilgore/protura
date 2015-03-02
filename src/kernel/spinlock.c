
#include <protura/types.h>
#include <protura/debug.h>
#include <protura/spinlock.h>

#include <arch/asm.h>

void spinlock_aquire(struct spinlock *lock)
{
    while (xchg(&lock->locked, 1) != 0)
        ;
}

void spinlock_release(struct spinlock *lock)
{
    xchg(&lock->locked, 0);
}

