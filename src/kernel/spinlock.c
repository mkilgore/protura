
#include <protura/types.h>
#include <protura/debug.h>
#include <protura/spinlock.h>

#include <arch/asm.h>

void spinlock_acquire(struct spinlock *lock)
{
    lock->eflags = eflags_read();
    cli();
    while (xchg(&lock->locked, 1) != 0)
        ;
}

void spinlock_release(struct spinlock *lock)
{
    xchg(&lock->locked, 0);
    eflags_write(lock->eflags);
}

