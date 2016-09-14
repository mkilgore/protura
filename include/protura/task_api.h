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

#define TASKIO_MEM_INFO 20

struct task_api_mem_region {
    uintptr_t start;
    uintptr_t end;

    unsigned int is_write :1;
    unsigned int is_read :1;
    unsigned int is_exec :1;
};

struct task_api_mem_info {
    pid_t pid;

    int region_count;
    struct task_api_mem_region regions[10];
};

#endif
