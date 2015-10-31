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
#include <protura/list.h>

#include <arch/spinlock.h>
#include <fs/simple_fs.h>
#include <fs/elf.h>
#include <fs/file_system.h>

static struct file_system_list {
    spinlock_t lock;
    list_head_t list;
} file_system_list = {
    .lock = SPINLOCK_INIT("file system list"),
    .list = LIST_HEAD_INIT(file_system_list.list),
};

void file_system_register(struct file_system *system)
{
    kprintf("Registering file system: %s\n", system->name);
    using_spinlock(&file_system_list.lock)
        list_add_tail(&file_system_list.list, &system->fs_list_entry);
}

void file_system_unregister(const char *name)
{
    struct file_system *system;

    kprintf("Unregistering file system: %s\n", name);

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

void file_systems_init(void)
{
    simple_fs_init();

    elf_register();
}

