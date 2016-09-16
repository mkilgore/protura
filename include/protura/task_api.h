#ifndef INCLUDE_PROTURA_TASK_API_H
#define INCLUDE_PROTURA_TASK_API_H

#if __KERNEL__
# include <protura/types.h>
#endif

#include <protura/signal.h>
#include <protura/fs/fdset.h>

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
#define TASKIO_FILE_INFO 21

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

struct task_api_file {
    short dev;
    short inode;
    int mode;
    int offset;
    int size;

    union {
        struct {
            unsigned int in_use :1;
            unsigned int is_pipe :1;
            unsigned int is_readable :1;
            unsigned int is_writable :1;
            unsigned int is_nonblock :1;
            unsigned int is_append :1;
            unsigned int is_cloexec :1;
        };
        unsigned int bitmap;
    };
};

struct task_api_file_info {
    pid_t pid;

    struct task_api_file files[NOFILE];
};

#endif
