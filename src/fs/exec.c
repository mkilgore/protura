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

/* The end result of all this argument shifting is to end up with the below
 * setup:
 *
 * ----- Stack end -----
 * NULL
 * envp[envc - 1]
 * envp[envc - 2]
 * ...
 * envp[1]
 * envp[0]
 * NULL
 * argv[argc - 1]
 * argv[argc - 2]
 * ...
 * argv[1]
 * argv[0]
 * contents of string envp[argc - 1]
 * contents of string envp[argc - 2]
 * ...
 * contents of string envp[1]
 * contents of string envp[0]
 * contents of string argv[argc - 1]
 * contents of string argv[argc - 2]
 * ...
 * contents of string argv[1]
 * contents of string argv[0]
 * envp pointer
 * argv pointer
 * argc integer
 * ... Usable stack space ...
 * ----- Stack beginning -----
 *
 * Because the arguments are passed in from user-space, however, we can't
 * simply copy from the old stack to the new stack - By the time the new stack
 * is created, the old program's memory is gone. Thus, we make a simple
 * temporary copy of all the arguments into kernel memory before loading the
 * binary. That copy is setup like this:
 *
 * ----- Stack end -----
 * NULL
 * envp[envc - 1] offset
 * envp[envc - 2] offset
 * ...
 * envp[1] offset
 * envp[0] offset
 * NULL
 * argv[argc - 1] offset
 * argv[argc - 2] offset
 * ...
 * argv[1] offset
 * argv[0] offset
 * contents of string envp[argc - 1]
 * contents of string envp[argc - 2]
 * ...
 * contents of string envp[1]
 * contents of string envp[0]
 * contents of string argv[argc - 1]
 * contents of string argv[argc - 2]
 * ...
 * contents of string argv[1]
 * contents of string argv[0]
 * envp offset
 * argv offset
 * argc integer
 * ----- Stack beginning -----
 *
 * Where the offsets indicate how far from the top of the stack (end of the
 * stack) that pointer refers to. Thus, this setup can be copied almost
 * directly to the user-space, with the offsets being replaced with pointers
 * which are simply (stack end - offset).
 */

static int execve(struct inode *inode, const char *file, const char *const argv[], const char *const envp[], struct irq_frame *frame)
{
    struct file *filp;
    struct exe_params params;
    int ret;
    char *sp;
    char *user_stack_end;
    struct task *current = cpu_get_local()->current;
    char new_name[128];
    size_t name_len = 0;
    const char *const *arg;

    exe_params_init(&params);

    strncpy(params.filename, file, sizeof(params.filename));
    params.filename[sizeof(params.filename) - 1] = '\0';

    if (argv == NULL)
        argv = (const char *[]) { file, NULL };

    if (envp == NULL)
        envp = (const char *[]) { file, NULL };

    ret = vfs_open(inode, F(FILE_READABLE), &filp);
    if (ret)
        return ret;

    name_len = snprintf(new_name, sizeof(new_name), "%s", file);

    /* We skip argv[0], as it should be the same as the filename above */
    if (*argv)
        for (arg = argv + 1; *arg && name_len < sizeof(new_name); arg++)
            name_len += snprintf(new_name + name_len, sizeof(new_name) - name_len, " %s", *arg);

    params_fill(&params, argv, envp);

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

  close_fd:
    params_clear(&params);
    vfs_close(filp);
    return ret;
}

static int verify_param_list(const char *const *list)
{
    int ret, i = 0;

    for (i = 0; list[i]; i++) {
        ret = user_check_region(list + i, sizeof(*list), F(VM_MAP_READ));
        if (ret)
            return ret;

        if (list[i]) {
            ret = user_check_strn(list[i], 256, F(VM_MAP_READ));
            if (ret)
                return ret;
        }
    }

    return 0;
}

int sys_execve(const char *file, const char *const argv[], const char *const envp[], struct irq_frame *frame)
{
    struct inode *exe;
    struct task *current = cpu_get_local()->current;
    int ret;

    ret = user_check_strn(file, 256, F(VM_MAP_READ));
    if (ret)
        return ret;

    if (argv) {
        ret = verify_param_list(argv);
        if (ret)
            return ret;
    }

    if (envp) {
        ret = verify_param_list(envp);
        if (ret)
            return ret;
    }

    kp(KP_TRACE, "Executing: %s\n", file);

    ret = namex(file, current->cwd, &exe);
    if (ret) {
        irq_frame_set_syscall_ret(frame, ret);
        return 0;
    }

    ret = execve(exe, file, argv, envp, frame);

    inode_put(exe);

    kp(KP_TRACE, "execve ret: %d\n", ret);
    if (ret)
        irq_frame_set_syscall_ret(frame, ret);

    return 0;
}

