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

#include <protura/block/bcache.h>
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
    pfree_va(pstr->arg, 0);
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

    pstr->arg = palloc_va(0, PAL_KERNEL);

    strncpy(pstr->arg, arg, PG_SIZE);

    if (pstr->arg[PG_SIZE - 1]) {
        pfree_va(pstr->arg, 0);
        kfree(pstr);
        return NULL;
    }

    pstr->len = strlen(pstr->arg);
    return pstr;
}

struct param_string *param_string_user_new(struct user_buffer buf, int *ret)
{
    struct param_string *pstr;

    pstr = kmalloc(sizeof(*pstr), PAL_KERNEL);
    param_string_init(pstr);

    pstr->arg = palloc_va(0, PAL_KERNEL);

    *ret = user_strncpy_to_kernel(pstr->arg, buf, PG_SIZE);
    if (*ret) {
        pfree_va(pstr->arg, 0);
        kfree(pstr);
        return NULL;
    }

    pstr->len = strlen(pstr->arg);
    return pstr;
}

static int params_add_user_arg_either(list_head_t *str_list, int *argc, struct user_buffer buf)
{
    int ret;
    struct param_string *pstr;

    pstr = param_string_user_new(buf, &ret);
    if (ret)
        return ret;

    (*argc)++;
    list_add_tail(str_list, &pstr->param_entry);

    return 0;
}

static int params_add_arg_either(list_head_t *str_list, int *argc, const char *arg)
{
    struct param_string *pstr;

    pstr = param_string_new(arg);
    if (!pstr)
        return -ENOMEM;

    (*argc)++;
    list_add_tail(str_list, &pstr->param_entry);

    return 0;
}

int params_add_arg(struct exe_params *params, const char *arg)
{
    return params_add_arg_either(&params->arg_params, &params->argc, arg);
}

int params_add_arg_first(struct exe_params *params, const char *arg)
{
    struct param_string *pstr;

    pstr = param_string_new(arg);
    if (!pstr)
        return -ENOMEM;

    params->argc++;
    list_add(&params->arg_params, &pstr->param_entry);

    return 0;
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

static int params_copy_user_strs(list_head_t *str_list, int *argc, struct user_buffer argv)
{
    int idx = 0;

    while (1) {
        char *str;
        int ret = user_copy_to_kernel_indexed(&str, argv, idx);
        if (ret)
            return ret;

        idx++;

        if (!str)
            break;

        struct user_buffer str_wrapped = user_buffer_make(str, argv.is_user);

        ret = params_add_user_arg_either(str_list, argc, str_wrapped);
        if (ret)
            return ret;
    }

    return 0;
}

static int params_copy_strs(list_head_t *str_list, int *argc, const char *const strs[])
{
    const char *const *arg;

    for (arg = strs; *arg; arg++) {
        int ret = params_add_arg_either(str_list, argc, *arg);
        if (ret)
            return ret;
    }

    return 0;
}

static void params_free_strs(list_head_t *str_list)
{
    struct param_string *pstr;

    list_foreach_take_entry(str_list, pstr, param_entry) {
        params_string_free(pstr);
        kfree(pstr);
    }
}

int params_fill_from_user(struct exe_params *params, struct user_buffer argv, struct user_buffer envp)
{
    int ret = params_copy_user_strs(&params->arg_params, &params->argc, argv);
    if (ret)
        return ret;

    return params_copy_user_strs(&params->env_params, &params->envc, envp);
}

int params_fill(struct exe_params *params, const char *const argv[], const char *const envp[])
{
    int ret = params_copy_strs(&params->arg_params, &params->argc, argv);
    if (ret)
        return ret;

    return params_copy_strs(&params->env_params, &params->envc, envp);
}

void params_clear(struct exe_params *params)
{
    params_free_strs(&params->arg_params);
    params_free_strs(&params->env_params);
}

