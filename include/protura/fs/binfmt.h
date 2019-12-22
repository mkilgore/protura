/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_BINFMT_H
#define INCLUDE_FS_BINFMT_H

#include <protura/types.h>
#include <protura/list.h>
#include <protura/atomic.h>
#include <protura/fs/file.h>
#include <protura/irq.h>

struct param_string {
    list_node_t param_entry;
    char *arg;
    size_t len;
};

#define PARAM_STRING_INIT(str) \
    { \
        .param_entry = LIST_NODE_INIT((str).param_entry) \
    }

static inline void param_string_init(struct param_string *pstr)
{
    *pstr = (struct param_string)PARAM_STRING_INIT(*pstr);
}

struct exe_params {
    struct file *exe;
    char filename[256];

    int argc;
    list_head_t arg_params;

    int envc;
    list_head_t env_params;
};

#define EXE_PARAMS_INIT(params) \
    { \
        .arg_params = LIST_HEAD_INIT((params).arg_params), \
        .env_params = LIST_HEAD_INIT((params).env_params), \
    }

static inline void exe_params_init(struct exe_params *params)
{
    *params = (struct exe_params)EXE_PARAMS_INIT(*params);
}

struct binfmt {
    list_node_t binfmt_list_entry;
    atomic_t use_count;
    const char *name;

    const char *magic;

    int (*load_bin) (struct exe_params *, struct irq_frame *);
};

#define BINFMT_INIT(fmt, fmtname, fmt_magic, loadbin) \
    { \
        .binfmt_list_entry = LIST_NODE_INIT((fmt).binfmt_list_entry), \
        .use_count = ATOMIC_INIT(0), \
        .name = fmtname, \
        .load_bin = (loadbin), \
        .magic = (fmt_magic), \
    }

static inline void binfmt_init(struct binfmt *fmt, const char *name, const char *magic, int (*load_bin) (struct exe_params *, struct irq_frame *))
{
    *fmt = (struct binfmt)BINFMT_INIT(*fmt, name, magic, load_bin);
}

void binfmt_register(struct binfmt *);
void binfmt_unregister(struct binfmt *);

/* Loads the binary specified by 'exe_params' into the current task */
int binary_load(struct exe_params *, struct irq_frame *);

extern const struct file_ops binfmt_file_ops;

void script_register(void);

void params_remove_args(struct exe_params *params, int count);
int params_add_arg(struct exe_params *params, const char *arg);
int params_fill(struct exe_params *params, const char *const argv[], const char *const envp[]);
int params_fill_from_user(struct exe_params *params, struct user_buffer argv, struct user_buffer envp);
void params_clear(struct exe_params *params);
int params_add_arg_first(struct exe_params *params, const char *arg);

char *params_copy_to_userspace(struct exe_params *params, char *ustack);

#endif
