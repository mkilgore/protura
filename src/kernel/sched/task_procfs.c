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

struct task_seq_state {
    struct task *current_task;
};

static int task_seq_start(struct seq_file *seq)
{
    struct task_seq_state *state = kzalloc(sizeof(*state), PAL_KERNEL);
    if (!state)
        return -ENOMEM;

    seq->priv = state;
    spinlock_acquire(&ktasks.lock);

    if (seq->iter_offset == 0)
        return 0;

    state->current_task = list_first_entry(&ktasks.list, struct task, task_list_node);

    int i;
    for (i = 1; i < seq->iter_offset; i++)
        state->current_task = list_next_entry(state->current_task, task_list_node);

    return 0;
}

static int task_seq_render(struct seq_file *seq)
{
    struct task_seq_state *state = seq->priv;

    if (!state->current_task)
        return seq_printf(seq, "Pid\tPPid\tPGid\tState\tKilled\tName\n");

    struct task *t = state->current_task;

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
    struct task_seq_state *state = seq->priv;

    if (!state->current_task)
        state->current_task = list_first_entry(&ktasks.list, struct task, task_list_node);
    else
        state->current_task = list_next_entry(state->current_task, task_list_node);

    seq->iter_offset++;

    if (list_ptr_is_head(&ktasks.list, &state->current_task->task_list_node)) {
        kp(KP_NORMAL, "Task rendering done!\n");
        flag_set(&seq->flags, SEQ_FILE_DONE);
    }

    return 0;
}

static void task_seq_end(struct seq_file *seq)
{
    struct task_seq_state *state = seq->priv;
    spinlock_release(&ktasks.lock);
    kfree(state);
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
