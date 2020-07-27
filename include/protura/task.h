#ifndef INCLUDE_PROTURA_TASK_H
#define INCLUDE_PROTURA_TASK_H

#include <protura/types.h>
#include <protura/errors.h>
#include <protura/list.h>
#include <protura/stddef.h>
#include <protura/compiler.h>
#include <protura/wait.h>
#include <protura/work.h>
#include <protura/mm/vm.h>
#include <protura/signal.h>
#include <protura/fs/fdset.h>
#include <protura/users.h>
#include <arch/context.h>
#include <arch/paging.h>
#include <arch/cpu.h>
#include <arch/task.h>

struct file;
struct inode;
struct tty;

/* Indicates the current state of a task. It should be noted that these states
 * are separate from preemption. Tasks can be preempted and restarted at any
 * time preemption is enabled, reguardless of task_sate. When a task explicetly
 * calls scheduler_yield(), the state is applied.
 *
 * TASK_INTR_SLEEPING is interruptible sleep. The different from TASK_SLEEPING
 * is that interruptible sleep can be ended by the process receiving a signal
 * or similar - which would ideally be handled immedieatly instead of
 * completing the current syscall. Without interuptible sleep, sleeping
 * processes would never handle signals until they are woken-up.
 */
enum task_state {
    TASK_NONE,
    TASK_SLEEPING,
    TASK_INTR_SLEEPING,
    TASK_RUNNING,
    TASK_STOPPED,
    TASK_ZOMBIE,
    TASK_DEAD,
};

enum {
    TASK_FLAG_PREEMPTED,
    TASK_FLAG_RUNNING,
    TASK_FLAG_KERNEL,
    TASK_FLAG_KILLED,
    TASK_FLAG_SESSION_LEADER,
    TASK_FLAG_RW_USER,
};

struct task {
    pid_t pid;
    pid_t pgid;
    pid_t session_id;

    enum task_state state;

    struct tty *tty;

    flags_t flags;

#define TASK_SIGNAL_STOP 0x100
#define TASK_SIGNAL_CONT 0x200

    int ret_code, ret_signal;

    list_node_t task_list_node;

    /* If this task is sleeping in a wait_queue, then this node is attached to
     * that wait_queue */
    struct wait_queue_node wait;

    struct address_space *addrspc;

    struct task *parent;

    list_node_t task_sibling_list;

    spinlock_t children_list_lock;
    list_head_t task_children;

    context_t context;
    void *kstack_bot, *kstack_top;
    int in_page_fault;

    struct file *files[NOFILE];
    struct inode *cwd;

    fd_set close_on_exec;

    mode_t umask;

    /* When modifying the sets, this lock must be taken */
    sigset_t sig_pending, sig_blocked;

    struct sigaction sig_actions[NSIG];

    struct credentials creds;

    struct arch_task_info arch_info;

    /* The address to jump too if a user fault happens while reading/writing
     * userspace */
    void *user_check_jmp_address;

    char name[128];
};

extern struct task *task_pid1;

/* Allocates a new task, assigning it a PID, intializing it's kernel
 * stack for it's first run, giving it a blank address_space, and setting the
 * state to TASK_RUNNING.
 *
 * The caller should do any other initalization, and then put the task into the
 * scheduler list using scheduler_task_add(). */
struct task *__must_check task_new(void);
struct task *__must_check task_fork(struct task *);
struct task *__must_check task_user_new_exec(const char *exe);
struct task *__must_check task_user_new(void);

void task_init(struct task *);

/* Used for the 'fork()' syscall */
pid_t __fork(struct task *current, pid_t pgrp);
pid_t sys_fork(void);
pid_t sys_fork_pgrp(pid_t pgrp); /* Fork and set pgrp - Protura exclusive */
pid_t sys_getpid(void);
pid_t sys_getppid(void);
pid_t sys_setsid(void);
pid_t sys_getsid(pid_t pid);
void sys_exit(int code) __noreturn;
int sys_dup(int oldfd);
int sys_dup2(int olfd, int newfd);
int sys_setpgid(pid_t pid, pid_t pgid);
int sys_getpgrp(struct user_buffer pgrp);

/* Used when a task is already killed and dead */
void task_free(struct task *);

/* Creates a new kernel-only task - it has no coresponding userspace, but
 * otherwise can do pretty much anything */
struct task *__must_check task_kernel_new(const char *name, int (*kernel_task) (void *), void *);
void task_kernel_init(struct task *t, const char *name, int (*kernel_task)(void *), void *ptr);

void task_print(char *buf, size_t size, struct task *);
void task_switch(context_t *old, struct task *new);

/* Turns the provided task into a 'zombie' - Closes all files, releases held
 * inode's, free's the address-space, etc... Free's everything except it's own
 * kernel stack. Then, it sets the tasks state to TASK_ZOMBIE. Zombie's are set
 * to be reaped by the parent. */
void task_make_zombie(struct task *t);

/* File descriptor related functions */

int task_fd_assign_empty(struct task *t, struct file *filp);
int task_fd_get_empty(struct task *t);
void task_fd_release(struct task *t, int fd);
#define task_fd_assign(t, fd, filp) \
    ((t)->files[(fd)] = (filp))
#define task_fd_get(t, fd) \
    (((intptr_t)(t)->files[(fd)] == -1)? NULL: (t)->files[(fd)])

static inline int task_fd_get_checked(struct task *t, int fd, struct file **filp)
{
    if (fd > NOFILE || fd < 0)
        return -EBADF;

    *filp = task_fd_get(t, fd);

    if (*filp == NULL)
        return -EBADF;

    return 0;
}

#define fd_get_empty() \
    task_fd_get_empty(cpu_get_local()->current)

#define fd_release(fd) \
    task_fd_release(cpu_get_local()->current, (fd));

#define fd_assign(fd, filp) \
    task_fd_assign(cpu_get_local()->current, (fd), (filp))

#define fd_assign_empty(filp) \
    task_fd_assign_empty(cpu_get_local()->current, filp)

#define fd_get(fd) \
    task_fd_get(cpu_get_local()->current, (fd))

#define fd_get_checked(fd, filp) \
    task_fd_get_checked(cpu_get_local()->current, (fd), (filp))

extern const char *task_states[];

static inline int signals_pending(void)
{
    struct task *t = cpu_get_local()->current;

    return t->sig_pending & ~t->sig_blocked;
}

int task_sleep_ms(int ms);
int task_sleep_intr_ms(int ms);
int sys_sleep(int seconds);
int sys_usleep(useconds_t us);

#endif
