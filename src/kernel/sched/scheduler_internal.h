#ifndef SRC_KERNEL_SCHED_SCHEDULER_INTERNAL_H
#define SRC_KERNEL_SCHED_SCHEDULER_INTERNAL_H

/* ktasks is the current list of tasks the scheduler is holding.
 *
 * 'list' is the current list of struct task tasks that the scheduler could schedule.
 *
 * 'dead' is a list of tasks that have been killed and need to be cleaned up.
 *      - The scheduler handles this because a task can't clean itself up from
 *        it's own task. For example, a task can't free it's own stack.
 *
 * 'next_pid' is the next pid to assign to a new 'struct task'.
 *
 * 'lock' is a spinlock that needs to be held when you modify the list of tasks.
 *
 * Note that the locking is a little tricky to understand. In some cases, a
 * standard using_spinlock, with the normal spinlock_acquire and
 * spinlock_release is perfectly fine. However, when the lock is going to
 * suround a context switch, it means that the lock itself is going to be
 * locked and unlocked in two different contexts. Thus, the eflags value that
 * is normally saved in the spinlock would be preserved across the context,
 * corrupting the interrupt state.
 *
 * The solution is to save the contents of the eflags register into the current
 * task's context, and then restore it on a context switch from the saved
 * value in the new task's context. Since we're saving the eflags ourselves,
 * it's necessary to call the 'noirq' versions of spinlock, which do nothing to
 * change the interrupt state.
 */
struct sched_task_list {
    struct spinlock lock;
    list_head_t list;

    list_head_t dead;

    pid_t next_pid;
};

extern struct sched_task_list ktasks;

#endif
