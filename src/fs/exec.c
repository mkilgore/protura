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
#include <arch/spinlock.h>
#include <protura/mutex.h>
#include <protura/atomic.h>
#include <protura/mm/kmalloc.h>
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
#include <protura/fs/exec.h>

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
 * Because the arguments are passed in from user-spaced, however, we can't
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

#define stack_push(sp, item) \
    do { \
        (sp) -= sizeof(item); \
        *(typeof(item) *)(sp) = item; \
    } while (0)

static int count_args(const char *const args[])
{
    int argc = 0;
    while (*args)
        argc++, args++;

    return argc;
}

static size_t create_arg_copy(char *sp, const char *const argv[], int argc, const char *const envp[], int envc)
{
    char *stack_top = sp;
    int i;
    char *user_argv;

    user_argv = sp;

    sp -= (argc + 1) * sizeof(char *);

    stack_push(user_argv, NULL);

    for (i = argc - 1; i >= 0; i--) {
        const char *s = argv[i];

        if (!s) {
            stack_push(user_argv, NULL);
            continue;
        }

        size_t s_len = strlen(s);

        /* Remove space from the stack for the string and the null terminator */
        sp -= s_len + 1;

        /* Put the offset to the start of this string into the argv array */
        stack_push(user_argv, stack_top - sp);

        /* Put the string into memory */
        memcpy(sp, s, s_len + 1);
    }

    /* Strings can be an odd length, so we have to manually align the stack
     * pointer to the alignment of the argv pointer */
    sp = ALIGN_2_DOWN(sp, alignof(char **));

    /* The first 'zero' is the argv pointer - It will be filled in by
     * copy_to_userspace */
    stack_push(sp, (int)0);
    stack_push(sp, argc);

    return stack_top - sp;
}

static char *copy_to_userspace(char *sp, char *copy, size_t len, int argc)
{
    char *stack_top = sp;
    char **argv;
    int i;

    memcpy(sp - len, copy - len, len);

    argv = (char **)sp - (argc + 1);

    for (i = 0; i < argc + 1; i++)
        if (argv[i])
            argv[i] = stack_top - (size_t)argv[i];

    /* Store the pointer to 'argv' on the stack */
    *(char ***)(sp - len + sizeof(int)) = argv;

    return sp - len;
}

int execve(struct inode *inode, const char *const argv[], const char *const envp[], struct irq_frame *frame)
{
    struct file *filp;
    struct exe_params params;
    int fd;
    int ret;
    int argc, envc;
    char *saved_args, *sp;
    char *user_stack_end;
    size_t arg_len;

    if (argv == NULL)
        argv = (const char *[]) { NULL };

    if (envp == NULL)
        envp = (const char *[]) { NULL };

    fd = __sys_open(inode, F(FILE_READABLE), &filp);
    if (fd < 0)
        return fd;

    saved_args = palloc_va(log2(CONFIG_KERNEL_ARG_PAGES), PAL_KERNEL);

    argc = count_args(argv);
    envc = count_args(envp);
    arg_len = create_arg_copy(saved_args + PG_SIZE * CONFIG_KERNEL_ARG_PAGES, argv, argc, envp, envc);

    params.exe = filp;
    ret = binary_load(&params, frame);

    if (ret)
        goto close_fd;

    user_stack_end = cpu_get_local()->current->addrspc->stack->addr.end;
    sp = copy_to_userspace(user_stack_end, saved_args + PG_SIZE * CONFIG_KERNEL_ARG_PAGES, arg_len, argc);

    irq_frame_set_stack(frame, sp);

  close_fd:
    pfree_va(saved_args, log2(CONFIG_KERNEL_ARG_PAGES));
    sys_close(fd);
    return ret;
}

int sys_execve(const char *file, const char *const argv[], const char *const envp[], struct irq_frame *frame)
{
    struct inode *exe;
    struct task *current = cpu_get_local()->current;
    int ret;

    ret = namex(file, current->cwd, &exe);
    if (ret)
        return ret;

    execve(exe, argv, envp, frame);

    inode_put(exe);

    return ret;
}

