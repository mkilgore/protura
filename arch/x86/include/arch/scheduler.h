#ifndef INCLUDE_ARCH_SCHEDULER_H
#define INCLUDE_ARCH_SCHEDULER_H

#include <arch/task.h>

pid_t scheduler_next_pid(void);

void scheduler_task_add(struct task *);
void scheduler_task_remove(struct task *);

void scheduler_task_yield(void);
void scheduler_task_sleep(uint32_t mseconds);

void scheduler(void);

/* Entry point for all new tasks */
void scheduler_task_entry(void);

#endif
