
#include <protura/types.h>
#include <protura/debug.h>

#include <arch/spinlock.h>
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

int spinlock_trylock(struct spinlock *lock)
{
    u32 eflags;
    int got_lock;

    eflags = eflags_read();
    cli();

    got_lock = xchg(&lock->locked, 1);

    if (got_lock)
        lock->eflags = eflags;
    else
        eflags_write(eflags);

    return got_lock;
}

