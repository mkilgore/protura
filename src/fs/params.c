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

#define stack_push(sp, item) \
    do { \
        (sp) -= sizeof(item); \
        *(typeof(item) *)(sp) = item; \
    } while (0)

static void params_string_free(struct param_string *pstr)
{
    kfree(pstr->arg);
}

void params_remove_args(struct exe_params *params, int count)
{
    int i = 0;
    struct param_string *pstr;

    list_foreach_take_entry(&params->arg_params, pstr, param_entry) {
        params->argc--;
        params_string_free(pstr);
        kfree(pstr);

        i++;
        if (i == count)
            break;
    }
}

struct param_string *param_string_new(const char *arg)
{
    struct param_string *pstr;

    pstr = kmalloc(sizeof(*pstr), PAL_KERNEL);
    param_string_init(pstr);
    pstr->len = strlen(arg);
    pstr->arg = kstrdup(arg, PAL_KERNEL);

    return pstr;
}

static void params_add_arg_either(list_head_t *str_list, int *argc, const char *arg)
{
    struct param_string *pstr;

    pstr = param_string_new(arg);

    (*argc)++;
    list_add_tail(str_list, &pstr->param_entry);
}

void params_add_arg(struct exe_params *params, const char *arg)
{
    params_add_arg_either(&params->arg_params, &params->argc, arg);
}

void params_add_arg_first(struct exe_params *params, const char *arg)
{
    struct param_string *pstr;

    pstr = param_string_new(arg);

    params->argc++;
    list_add(&params->arg_params, &pstr->param_entry);
}

static char *params_copy_list(list_head_t *list, char **argv, char *ustack)
{
    int i;
    struct param_string *pstr;

    i = 0;
    list_foreach_entry(list, pstr, param_entry) {
        kp(KP_TRACE, "Arg str: %s\n", pstr->arg);
        ustack -= pstr->len + 1;
        memcpy(ustack, pstr->arg, pstr->len + 1);

        argv[i] = ustack;
        i++;
    }

    return ustack;
}

char *params_copy_to_userspace(struct exe_params *params, char *ustack)
{
    char **envp, **argv;

    envp = (char **)ustack - (params->envc + 1);
    argv = envp - (params->argc + 1);

    argv[params->argc] = NULL;
    envp[params->envc] = NULL;

    ustack = (char *)(argv - 1);

    ustack = params_copy_list(&params->arg_params, argv, ustack);
    ustack = params_copy_list(&params->env_params, envp, ustack);

    stack_push(ustack, envp);
    stack_push(ustack, argv);
    stack_push(ustack, params->argc);

    /* kp(KP_TRACE, "argc: %d, envc: %d\n", params->argc, params->envc); */

    return ustack;
}

static void params_copy_strs(list_head_t *str_list, int *argc, const char *const strs[])
{
    const char *const *arg;

    for (arg = strs; *arg; arg++)
        params_add_arg_either(str_list, argc, *arg);
}

static void params_free_strs(list_head_t *str_list)
{
    struct param_string *pstr;

    list_foreach_take_entry(str_list, pstr, param_entry) {
        params_string_free(pstr);
        kfree(pstr);
    }
}

void params_fill(struct exe_params *params, const char *const argv[], const char *const envp[])
{
    params_copy_strs(&params->arg_params, &params->argc, argv);
    params_copy_strs(&params->env_params, &params->envc, envp);

    kp(KP_TRACE, "argc: %d, envc: %d\n", params->argc, params->envc);
}

void params_clear(struct exe_params *params)
{
    params_free_strs(&params->arg_params);
    params_free_strs(&params->env_params);
}

