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

struct exe_params {
    struct file *exe;
};

struct binfmt {
    list_node_t binfmt_list_entry;
    atomic_t use_count;
    const char *name;

    int (*load_bin) (struct exe_params *, struct irq_frame *);
};

#define BINFMT_INIT(fmt, fmtname, loadbin) \
    { \
        .binfmt_list_entry = LIST_NODE_INIT((fmt).binfmt_list_entry), \
        .use_count = ATOMIC_INIT(0), \
        .name = fmtname, \
        .load_bin = (loadbin), \
    }

static inline void binfmt_init(struct binfmt *fmt, const char *name, int (*load_bin) (struct exe_params *, struct irq_frame *))
{
    *fmt = (struct binfmt)BINFMT_INIT(*fmt, name, load_bin);
}

void binfmt_register(struct binfmt *);
void binfmt_unregister(struct binfmt *);

/* Loads the binary specified by 'exe_params' into the current task */
int binary_load(struct exe_params *, struct irq_frame *);

#endif
