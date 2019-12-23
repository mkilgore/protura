/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/list.h>
#include <protura/hlist.h>
#include <protura/string.h>
#include <protura/snprintf.h>
#include <arch/spinlock.h>
#include <protura/mutex.h>
#include <protura/atomic.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/user_check.h>
#include <protura/signal.h>
#include <arch/task.h>

#include <protura/fs/block.h>
#include <protura/fs/super.h>
#include <protura/fs/file.h>
#include <protura/fs/stat.h>
#include <protura/fs/inode.h>
#include <protura/fs/namei.h>
#include <protura/fs/sys.h>
#include <protura/fs/vfs.h>
#include <protura/fs/binfmt.h>

static int check_credentials(struct inode *inode, struct task *current)
{
    using_creds(&current->creds) {
        if (inode->mode & S_IXOTH)
            return 0;

        if (inode->mode & S_IXGRP && __credentials_belong_to_gid(&current->creds, inode->gid))
            return 0;

        if (inode->mode & S_IXUSR && inode->uid == current->creds.euid)
            return 0;

        /* FIXME: We can't do this because we don't offer a chmod to set the X bit */
        // return -EACCES;
        return 0;
    }
}

static void set_credentials(struct inode *inode, struct task *current)
{
    uid_t new_euid;
    gid_t new_egid;

    using_creds(&current->creds) {
        new_euid = current->creds.euid;
        new_egid = current->creds.egid;

        if (inode->mode & S_ISUID)
            new_euid = inode->uid;

        if (inode->mode & S_ISGID)
            new_egid = inode->gid;

        current->creds.euid = current->creds.suid = new_euid;
        current->creds.egid = current->creds.sgid = new_egid;
    }
}

static void generate_task_name(char *name, size_t len, const char *file, struct user_buffer argv_buf)
{
    size_t name_len = 0;
    struct user_buffer arg;
    char *str = NULL;

    /* This is the creation of the replacement name for the current task */
    name_len = snprintf(name, len, "%s", file);

    int err = user_copy_to_kernel(&str, argv_buf);
    if (err)
        return;

    if (!str)
        return;

    /* We skip argv[0], as it should be the same as the filename above */
    for (arg = user_buffer_index(argv_buf, sizeof(char *)); !user_buffer_is_null(arg); arg = user_buffer_index(arg, sizeof(char *))) {
        err = user_copy_to_kernel(&str, arg);
        if (err)
            break;

        if (!str)
            break;

        struct user_buffer str_wrapped = user_buffer_make(str, arg.is_user);

        char arg_str[64];
        err = user_strncpy_to_kernel(arg_str, str_wrapped, sizeof(arg_str));
        if (err)
            break;

        name_len += snprintf(name + name_len, len - name_len, " %s", arg_str);
    }
}

static int execve(struct inode *inode, const char *file, struct user_buffer argv_buf, struct user_buffer envp_buf, struct irq_frame *frame)
{
    struct file *filp;
    struct exe_params params;
    int ret;
    char *sp;
    char *user_stack_end;
    struct task *current = cpu_get_local()->current;
    char new_name[128];

    const char *def_argv[] = { file, NULL };
    const char *def_envp[] = { NULL };

    exe_params_init(&params);

    strncpy(params.filename, file, sizeof(params.filename));
    params.filename[sizeof(params.filename) - 1] = '\0';

    if (user_buffer_is_null(argv_buf))
        argv_buf = make_kernel_buffer(def_argv);

    if (user_buffer_is_null(envp_buf))
        envp_buf = make_kernel_buffer(def_envp);

    generate_task_name(new_name, sizeof(new_name), file, argv_buf);

    ret = params_fill_from_user(&params, argv_buf, envp_buf);
    if (ret) {
        params_clear(&params);
        return ret;
    }

    ret = vfs_open(inode, F(FILE_READABLE), &filp);
    if (ret)
        return ret;

    params.exe = filp;
    ret = binary_load(&params, frame);

    kp(KP_TRACE, "binary_load: %d\n", ret);

    if (ret)
        goto close_fd;

    /* At this point, the pointers we were passed are now completely invalid
     * (besides frame and inode, which reside in the kernel). */

    user_stack_end = cpu_get_local()->current->addrspc->stack->addr.end;
    sp = params_copy_to_userspace(&params, user_stack_end);

    irq_frame_set_stack(frame, sp);

    int i;
    for (i = 0; i < NSIG; i++) {
        if (current->sig_actions[i].sa_handler != SIG_IGN)
            current->sig_actions[i].sa_handler = SIG_DFL;

        current->sig_actions[i].sa_mask = 0;
        current->sig_actions[i].sa_flags = 0;
    }

    for (i = 0; i < NOFILE; i++) {
        if (FD_ISSET(i, &current->close_on_exec)) {
            kp(KP_TRACE, "Close-on-exec: %d\n", i);
            sys_close(i);
        }
    }

    strcpy(current->name, new_name);

    set_credentials(inode, current);

  close_fd:
    params_clear(&params);
    vfs_close(filp);
    return ret;
}

int sys_execve(struct user_buffer file_buf, struct user_buffer argv_buf, struct user_buffer envp_buf, struct irq_frame *frame)
{
    struct inode *exe;
    struct task *current = cpu_get_local()->current;
    int ret;

    __cleanup_user_string char *tmp_file = NULL;
    ret = user_alloc_string(file_buf, &tmp_file);
    if (ret)
        return ret;

    kp(KP_TRACE, "Executing: %s\n", tmp_file);

    ret = namex(tmp_file, current->cwd, &exe);
    if (ret) {
        irq_frame_set_syscall_ret(frame, ret);
        return 0;
    }

    ret = check_credentials(exe, current);
    if (ret) {
        inode_put(exe);
        irq_frame_set_syscall_ret(frame, ret);
        return 0;
    }

    ret = execve(exe, tmp_file, argv_buf, envp_buf, frame);

    inode_put(exe);

    kp(KP_TRACE, "execve ret: %d\n", ret);
    if (ret)
        irq_frame_set_syscall_ret(frame, ret);

    return 0;
}

