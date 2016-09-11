#ifndef INCLUDE_PROTURA_TASK_API_H
#define INCLUDE_PROTURA_TASK_API_H

#if __KERNEL__
# include <protura/types.h>
# include <protura/signal.h>
# include <protura/fs/fdset.h>
#endif

enum task_api_state {
    TASK_API_NONE,
    TASK_API_SLEEPING,
    TASK_API_INTR_SLEEPING,
    TASK_API_RUNNING,
    TASK_API_ZOMBIE,
};

struct task_api_info {
    pid_t pid;
    pid_t ppid;
    pid_t pgid;
    enum task_api_state state;

    fd_set close_on_exec;
    sigset_t sig_pending, sig_blocked;

    unsigned int is_kernel :1;

    char name[128];
};

#endif
