#ifndef INCLUDE_ARCH_TASK_H
#define INCLUDE_ARCH_TASK_H

#include <protura/types.h>
#include <protura/list.h>
#include <protura/stddef.h>
#include <protura/wait.h>
#include <arch/context.h>
#include <arch/paging.h>

#define NOFILE 20

enum task_state {
    TASK_NONE,
    TASK_SLEEPING,
    TASK_RUNNABLE,
    TASK_RUNNING,
    TASK_DEAD,
};

struct task {
    kpid_t pid;
    enum task_state state;
    unsigned int preempted :1;

    int wake_up; /* Tick number to wake-up on */

    struct list_node task_list_node;

    /* If this task is sleeping in a wait_queue, then this node is attached to that wait_queue */
    struct wait_queue_node wait;

    pd_t *page_dir;

    struct task *parent;
    context_t context;
    void *kstack_bot, *kstack_top;

    int killed; /* If non-zero, we've been killed and need to exit() */

    char name[20];
};

void task_init(void);

/* Allocates a new task, assigning it a PID, intializing it's kernel
 * stack for it's first run, giving it a blank page-directory, and setting the
 * state to TASK_EMBRYO.
 *
 * The caller should do any other initalization, set the state to
 * TASK_RUNNABLE, and then put the task into the scheduler list using
 * task_add. */
struct task *task_new(void);
struct task *task_fork(struct task *);

/* Used when a task is already killed and dead */
void task_free(struct task *);

void task_paging_init(struct task *);
void task_paging_free(struct task *);

void task_paging_copy_user(struct task *restrict new, struct task *restrict old);
struct task *task_fake_create(void);

struct task *task_kernel_new(char *name, int (*kernel_task)(int argc, const char **argv), int argc, const char **argv);
struct task *task_kernel_new_interruptable(char *name, int (*kernel_task)(int argc, const char **argv), int argc, const char **argv);

void task_print(char *buf, ksize_t size, struct task *);
void task_switch(context_t *old, struct task *new);

extern const char *task_states[];

#endif
