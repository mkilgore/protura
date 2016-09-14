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
#include <protura/fs/elf.h>
#include <protura/fs/binfmt.h>
#include <protura/fs/file_system.h>

static struct file_system_list {
    spinlock_t lock;
    list_head_t list;
} file_system_list = {
    .lock = SPINLOCK_INIT("file system list"),
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

static int file_system_list_read(void *page, size_t page_size, size_t *len)
{
    struct file_system *system;

    *len = 0;

    using_spinlock(&file_system_list.lock)
        list_foreach_entry(&file_system_list.list, system, fs_list_entry)
            *len += snprintf(page + *len, page_size - *len, "%s\n", system->name);

    return 0;
}

struct procfs_entry_ops file_system_ops = {
    .readpage = file_system_list_read,
};

void file_systems_init(void)
{
    ext2_init();
    procfs_init();

    elf_register();
    script_register();
}

