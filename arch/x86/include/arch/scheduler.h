#ifndef INCLUDE_ARCH_SCHEDULER_H
#define INCLUDE_ARCH_SCHEDULER_H

#include <arch/task.h>
#include <protura/list.h>

pid_t scheduler_next_pid(void);

void scheduler_task_add(struct task *);
void scheduler_task_remove(struct task *);

void scheduler_task_yield(void);
void scheduler_task_sleep(uint32_t mseconds);

void scheduler(void);

/* Entry point for all new tasks */
void scheduler_task_entry(void);

#define WAKEUP_LIST_MAX_TASKS 20

struct wakeup_list {
    struct task *tasks[WAKEUP_LIST_MAX_TASKS];
};

void wakeup_list_init(struct wakeup_list *);
void wakeup_list_add(struct wakeup_list *, struct task *);
void wakeup_list_remove(struct wakeup_list *, struct task *);
void wakeup_list_wakeup(struct wakeup_list *);

#endif
