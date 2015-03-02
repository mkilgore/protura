#ifndef INCLUDE_PROTURA_SPINLOCK_H
#define INCLUDE_PROTURA_SPINLOCK_H

struct spinlock {
    unsigned int locked;

    char *name;
};

#define SPINLOCK_INIT(nam) { .locked = 0, .name = nam }

void spinlock_aquire(struct spinlock *);
void spinlock_release(struct spinlock *);

#endif
