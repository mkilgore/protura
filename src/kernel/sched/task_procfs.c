/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/list.h>
#include <protura/mm/kmalloc.h>
#include <protura/fs/procfs.h>
#include <protura/task_api.h>
#include <protura/fs/file.h>
#include <protura/fs/seq_file.h>

#include <protura/task.h>
#include <protura/scheduler.h>
#include "scheduler_internal.h"

static int task_seq_start(struct seq_file *seq)
{
    spinlock_acquire(&ktasks.lock);

    return seq_list_start_header(seq, &ktasks.list);
}

static int task_seq_render(struct seq_file *seq)
{
    struct task *t = seq_list_get_entry(seq, struct task, task_list_node);
    if (!t)
        return seq_printf(seq, "Pid\tPPid\tPGid\tState\tKilled\tName\n");

    return seq_printf(seq, "%d\t%d\t%d\t%s\t%d\t\"%s\"\n",
            t->pid,
            (t->parent)? t->parent->pid: 0,
            t->pgid,
            task_states[t->state],
            flag_test(&t->flags, TASK_FLAG_KILLED),
            t->name);
}

static int task_seq_next(struct seq_file *seq)
{
    return seq_list_next(seq, &ktasks.list);
}

static void task_seq_end(struct seq_file *seq)
{
    spinlock_release(&ktasks.lock);
}

const static struct seq_file_ops task_seq_file_ops = {
    .start = task_seq_start,
    .next = task_seq_next,
    .render = task_seq_render,
    .end = task_seq_end,
};

static int task_file_seq_open(struct inode *inode, struct file *filp)
{
    return seq_open(filp, &task_seq_file_ops);
}

struct file_ops task_file_ops = {
    .open = task_file_seq_open,
    .lseek = seq_lseek,
    .read = seq_read,
    .release = seq_release,
};
