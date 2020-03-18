/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef __INCLUDE_UAPI_PROTURA_TASK_API_H__
#define __INCLUDE_UAPI_PROTURA_TASK_API_H__

#include <protura/types.h>

#include <protura/signal.h>
#include <protura/fs/fdset.h>

enum task_api_state {
    TASK_API_NONE,
    TASK_API_SLEEPING,
    TASK_API_INTR_SLEEPING,
    TASK_API_RUNNING,
    TASK_API_STOPPED,
    TASK_API_ZOMBIE,
};

struct task_api_info {
    __kpid_t pid;
    __kpid_t ppid;
    __kpid_t pgid;
    __kpid_t sid;
    __kuid_t uid;
    __kgid_t gid;
    enum task_api_state state;

    __kfd_set close_on_exec;
    sigset_t sig_pending, sig_blocked;

    unsigned int is_kernel :1;
    unsigned int has_tty :1;

    __kudev_t tty_devno;
    char name[128];
};

#define TASKIO_MEM_INFO 20
#define TASKIO_FILE_INFO 21

struct task_api_mem_region {
    __kuintptr_t start;
    __kuintptr_t end;

    unsigned int is_write :1;
    unsigned int is_read :1;
    unsigned int is_exec :1;
};

struct task_api_mem_info {
    __kpid_t pid;

    int region_count;
    struct task_api_mem_region regions[10];
};

struct task_api_file {
    __kudev_t dev;
    __kino_t inode;
    __kmode_t mode;
    __koff_t offset;
    __koff_t size;

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
    __kpid_t pid;

    struct task_api_file files[__kNOFILE];
};

#endif
