#ifndef INCLUDE_PROTURA_TASK_H
#define INCLUDE_PROTURA_TASK_H

#include <protura/types.h>
#include <protura/list.h>
#include <protura/stddef.h>
#include <fs/file.h>
#include <fs/vfsnode.h>
#include <arch/paging.h>
#include <arch/idt.h>
#include <arch/gdt.h>
#include <arch/context.h>

#define NOFILE 20

enum task_state {
    TASK_UNUSED,
    TASK_EMBRYO,
    TASK_SLEEPING,
    TASK_RUNNABLE,
    TASK_RUNNING,
    TASK_ZOMBIE,
};

struct task {
    pid_t pid;
    enum task_state state;
    struct list_node task_list_node;

    struct page_directory *page_dir;

    struct task *parent;
    struct arch_context context;

    int killed; /* If non-zero, we've been killed */

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
struct task *task_new(char *name);
struct task *task_fork(struct task *);

/* Add and remove a task from the list of tasks to schedule time for. Only
 * tasks added via 'task_add' will be given a time-slice from the scheduler. */
void task_add(struct task *);
void task_remove(struct task *);

void task_paging_init(struct task *);
void task_paging_free(struct task *);

void task_paging_copy_user(struct task *restrict new, struct task *restrict old);
struct task *task_fake_create(void);
void task_yield(void);

void scheduler(struct idt_frame *frame);

void task_print(char *buf, size_t size, struct task *);
void task_switch(struct task *old, struct task *new, struct idt_frame *frame);
void task_start_init(void);

static inline void task_start(struct task *t, uint32_t flags)
{
    void *__context = &t->context.save_regs;

    asm volatile (
        "pushl $%c3\n"
        "pushl %0\n"
        "pushl %5\n"
        "pushl $%c4\n"
        "pushl %1\n"

        "pushl 44(%2)\n"
        "pushl 40(%2)\n"
        "pushl 36(%2)\n"
        "pushl 32(%2)\n"
        "pushl 28(%2)\n"
        "pushl 24(%2)\n"
        "pushl 20(%2)\n"
        "pushl 16(%2)\n"
        "pushl 12(%2)\n"
        "pushl 8(%2)\n"
        "pushl 4(%2)\n"
        "pushl (%2)\n"

        "popal\n"
        "popl %%gs\n"
        "popl %%fs\n"
        "popl %%es\n"
        "popl %%ds\n"
        "iret\n"
        : /* No outputs */
        : "r" (t->context.esp), "r" (t->context.eip),
          "r" (__context),      "i" (_USER_DS | DPL_USER),
          "i" (_USER_CS | DPL_USER), "r" (flags)
    );
}

#endif
