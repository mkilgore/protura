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
void task_schedule_add(struct task *);
void task_schedule_remove(struct task *);

void task_paging_init(struct task *);
void task_paging_free(struct task *);

void task_paging_copy_user(struct task *restrict new, struct task *restrict old);
struct task *task_fake_create(void);
struct task *task_new_kernel(char *name, int (*kernel_task)(int argc, const char **argv), int argc, const char **argv);
void task_yield(void);

void scheduler(struct idt_frame *frame);

void task_print(char *buf, size_t size, struct task *);
void task_switch(struct task *old, struct task *new, struct idt_frame *frame);
void task_start_init(void);

/* This code is used to jump-start a task by directly 'iret'ing into it. This
 * is really only useful for starting 'init'. */
static inline void task_start(struct task *t)
{
    void *__context = &t->context.save_regs;


    /* 'iret' expects 5 stack variables, which are the instruction-pointer to
     * use, the code-segment to use, eflags to use, stack pointer to use, and
     * stack-segment to use */
    asm volatile (
        /* We push the 'iret' parameters first, in reverse order */
        "pushl %[c_ss]\n"
        "pushl %[c_esp]\n"
        "pushl %[c_eflags]\n"
        "pushl %[c_cs]\n"
        "pushl %[c_eip]\n"

        /* Push the 'x86_regs' struct onto the stack, in reverse order. The
         * four segment registers %gs, %fs, %es, %ds are first, followed by the
         * layout of a 'pusha' instruction. */
        "pushl 44(%[context])\n"
        "pushl 40(%[context])\n"
        "pushl 36(%[context])\n"
        "pushl 32(%[context])\n"
        "pushl 28(%[context])\n"
        "pushl 24(%[context])\n"
        "pushl 20(%[context])\n"
        "pushl 16(%[context])\n"
        "pushl 12(%[context])\n"
        "pushl 8(%[context])\n"
        "pushl 4(%[context])\n"
        "pushl (%[context])\n"

        /* Use popal to load all the register values we just pushed into the
         * stack */
        "popal\n"
        /* Pop the four segment registers we pushed */
        "popl %%gs\n"
        "popl %%fs\n"
        "popl %%es\n"
        "popl %%ds\n"

        /* 'iret' using the parameters pushed at the top of the code */
        "iret\n"
        : /* No outputs */
        : [c_esp]    "r" (t->context.esp),
          [c_eip]    "r" (t->context.eip),
          [context] "r" (__context),
          [c_ss]     "r" ((uint32_t)t->context.ss),
          [c_cs]     "r" ((uint32_t)t->context.cs),
          [c_eflags] "r" (t->context.eflags)
    );
}

#endif
