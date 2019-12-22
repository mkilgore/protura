/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_SEMAPHORE_H
#define INCLUDE_PROTURA_SEMAPHORE_H

#include <protura/debug.h>
#include <protura/string.h>
#include <arch/spinlock.h>
#include <protura/list.h>

struct semaphore {
    spinlock_t lock;

    int count;
    list_head_t queue;
};

struct semaphore_wait_entry {
    list_node_t next;
    struct task *task;
};

typedef struct semaphore semaphore_t;

#define SEM_INIT(sem, cnt) \
    { .lock = SPINLOCK_INIT(), \
      .count = (cnt), \
      .queue = LIST_HEAD_INIT((sem).queue) }

static inline void sem_init(struct semaphore *sem, int value)
{
    memset(sem, 0, sizeof(*sem));

    spinlock_init(&sem->lock);
    sem->count = value;
    list_head_init(&sem->queue);
}

static inline void sem_wait_entry_init(struct semaphore_wait_entry *ent)
{
    list_node_init(&ent->next);
    ent->task = NULL;
}

void sem_up(struct semaphore *sem);
void sem_down(struct semaphore *sem);
int sem_try_down(struct semaphore *sem);
int sem_waiting(struct semaphore *sem);

#endif
