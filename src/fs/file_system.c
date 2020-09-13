/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/snprintf.h>
#include <protura/list.h>

#include <arch/spinlock.h>
#include <protura/fs/ext2.h>
#include <protura/fs/procfs.h>
#include <protura/fs/seq_file.h>
#include <protura/fs/elf.h>
#include <protura/fs/binfmt.h>
#include <protura/fs/file_system.h>

static struct file_system_list {
    spinlock_t lock;
    list_head_t list;
} file_system_list = {
    .lock = SPINLOCK_INIT(),
    .list = LIST_HEAD_INIT(file_system_list.list),
};

void file_system_register(struct file_system *system)
{
    kp(KP_NORMAL, "Registering file system: %s\n", system->name);
    using_spinlock(&file_system_list.lock)
        list_add_tail(&file_system_list.list, &system->fs_list_entry);
}

void file_system_unregister(const char *name)
{
    struct file_system *system;

    kp(KP_NORMAL, "Unregistering file system: %s\n", name);

    using_spinlock(&file_system_list.lock) {
        list_foreach_entry(&file_system_list.list, system, fs_list_entry) {
            if (strcmp(system->name, name) == 0) {
                list_del(&system->fs_list_entry);
                break;
            }
        }
    }
}

struct file_system *file_system_lookup(const char *name)
{
    struct file_system *found = NULL, *system;

    using_spinlock(&file_system_list.lock) {
        list_foreach_entry(&file_system_list.list, system, fs_list_entry) {
            if (strcmp(system->name, name) == 0) {
                found = system;
                break;
            }
        }
    }

    return found;
}

static int file_system_seq_start(struct seq_file *seq)
{
    spinlock_acquire(&file_system_list.lock);
    return seq_list_start(seq, &file_system_list.list);
}

static void file_system_seq_end(struct seq_file *seq)
{
    spinlock_release(&file_system_list.lock);
}

static int file_system_seq_render(struct seq_file *seq)
{
    struct file_system *file_system = seq_list_get_entry(seq, struct file_system, fs_list_entry);
    return seq_printf(seq, "%s\n", file_system->name);
}

static int file_system_seq_next(struct seq_file *seq)
{
    return seq_list_next(seq, &file_system_list.list);
}

const static struct seq_file_ops file_system_seq_file_ops = {
    .start = file_system_seq_start,
    .end = file_system_seq_end,
    .render = file_system_seq_render,
    .next = file_system_seq_next,
};

static int file_system_file_seq_open(struct inode *inode, struct file *filp)
{
    return seq_open(filp, &file_system_seq_file_ops);
}

const struct file_ops file_system_file_ops = {
    .open = file_system_file_seq_open,
    .lseek = seq_lseek,
    .read = seq_read,
    .release = seq_release,
};
