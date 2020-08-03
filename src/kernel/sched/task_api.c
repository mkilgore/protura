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
#include <protura/dev.h>
#include <protura/snprintf.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/user_check.h>
#include <protura/drivers/tty.h>
#include <protura/fs/procfs.h>
#include <protura/task_api.h>
#include <protura/fs/file.h>
#include <protura/block/bdev.h>

#include <protura/task.h>
#include <protura/scheduler.h>
#include "scheduler_internal.h"


static void __fill_task_api_info(struct task_api_info *tinfo, struct task *task)
{
    memset(tinfo, 0, sizeof(*tinfo));

    tinfo->is_kernel = flag_test(&task->flags, TASK_FLAG_KERNEL);
    tinfo->pid = task->pid;

    if (task->parent)
        tinfo->ppid = task->parent->pid;
    else
        tinfo->ppid = 0;

    tinfo->pgid = task->pgid;
    tinfo->sid = task->session_id;

    struct credentials *creds = &task->creds;

    using_creds(creds) {
        tinfo->uid = creds->uid;
        tinfo->gid = creds->gid;
    }

    if (task->tty) {
        tinfo->has_tty = 1;
        tinfo->tty_devno = task->tty->device_no;
    }

    switch (task->state) {
    case TASK_RUNNING:
        tinfo->state = TASK_API_RUNNING;
        break;

    case TASK_ZOMBIE:
        tinfo->state = TASK_API_ZOMBIE;
        break;

    case TASK_SLEEPING:
        tinfo->state = TASK_API_SLEEPING;
        break;

    case TASK_INTR_SLEEPING:
        tinfo->state = TASK_API_INTR_SLEEPING;
        break;

    case TASK_STOPPED:
        tinfo->state = TASK_API_STOPPED;
        break;

    case TASK_DEAD:
    case TASK_NONE:
        tinfo->state = TASK_API_NONE;
        break;
    }

    memcpy(&tinfo->close_on_exec, &task->close_on_exec, sizeof(tinfo->close_on_exec));
    memcpy(&tinfo->sig_pending, &task->sig_pending, sizeof(tinfo->sig_pending));
    memcpy(&tinfo->sig_blocked, &task->sig_blocked, sizeof(tinfo->sig_blocked));

    memcpy(tinfo->name, task->name, sizeof(tinfo->name));
}

static int scheduler_task_api_read(struct file *filp, struct user_buffer buf, size_t size)
{
    struct task_api_info tinfo;
    struct task *task, *found = NULL;
    pid_t last_pid = filp->offset;
    pid_t found_pid = -1;

    using_spinlock(&ktasks.lock) {
        list_foreach_entry(&ktasks.list, task, task_list_node) {
            if (task->state == TASK_DEAD
                || task->state == TASK_NONE)
                continue;

            if (task->pid > last_pid
                && (found_pid == -1 || task->pid < found_pid)) {
                found = task;
                found_pid = task->pid;
            }
        }

        if (!found)
            break;

        __fill_task_api_info(&tinfo, found);
    }

    if (found) {
        filp->offset = found_pid;
        int ret;

        if (size > sizeof(tinfo))
            ret = user_memcpy_from_kernel(buf, &tinfo, sizeof(tinfo));
        else
            ret = user_memcpy_from_kernel(buf, &tinfo, size);

        if (ret)
            return ret;

        return size;
    } else {
        return 0;
    }
}

static int task_api_fill_mem_info(struct task_api_mem_info *info)
{
    struct vm_map *map;
    struct task *task;
    int region;

    task = scheduler_task_get(info->pid);
    if (!task)
        return -ESRCH;

    list_foreach_entry(&task->addrspc->vm_maps, map, address_space_entry) {
        region = info->region_count++;

        if (region > ARRAY_SIZE(info->regions))
            break;

        info->regions[region].start = (uintptr_t)map->addr.start;
        info->regions[region].end = (uintptr_t)map->addr.end;

        info->regions[region].is_read= flag_test(&map->flags, VM_MAP_READ);
        info->regions[region].is_write = flag_test(&map->flags, VM_MAP_WRITE);
        info->regions[region].is_exec = flag_test(&map->flags, VM_MAP_EXE);
    }

    scheduler_task_put(task);

    return 0;
}

static int task_api_fill_file_info(struct task_api_file_info *info)
{
    int i;
    struct task *task;

    task = scheduler_task_get(info->pid);
    if (!task)
        return -ESRCH;

    for (i = 0; i < NOFILE; i++) {
        struct file *filp = task_fd_get(task, i);

        if (!filp) {
            info->files[i].in_use = 0;
            continue;
        }

        info->files[i].in_use = 1;

        if (inode_is_pipe(filp->inode))
            info->files[i].is_pipe = 1;

        if (flag_test(&filp->flags, FILE_WRITABLE))
            info->files[i].is_writable = 1;

        if (flag_test(&filp->flags, FILE_READABLE))
            info->files[i].is_readable = 1;

        if (flag_test(&filp->flags, FILE_NONBLOCK))
            info->files[i].is_nonblock= 1;

        if (flag_test(&filp->flags, FILE_APPEND))
            info->files[i].is_append = 1;

        info->files[i].inode = filp->inode->ino;
        info->files[i].dev = filp->inode->sb->bdev->dev;
        info->files[i].mode = filp->inode->mode;
        info->files[i].offset = filp->offset;
        info->files[i].size = filp->inode->size;
    }

    scheduler_task_put(task);

    return 0;
}

static int scheduler_task_api_ioctl(struct file *filp, int cmd, struct user_buffer ptr)
{
    struct task_api_mem_info *mem_info = NULL;
    struct task_api_file_info *file_info = NULL;
    int ret;

    switch (cmd) {
    case TASKIO_MEM_INFO:
        mem_info = kmalloc(sizeof(*mem_info), PAL_KERNEL);
        if (!mem_info)
            return -ENOMEM;

        ret = task_api_fill_mem_info(mem_info);
        if (ret) {
            kfree(mem_info);
            return ret;
        }

        ret = user_copy_from_kernel(ptr, *mem_info);

        kfree(mem_info);
        return ret;

    case TASKIO_FILE_INFO:
        file_info = kmalloc(sizeof(*file_info), PAL_KERNEL);
        if (!file_info)
            return -ENOMEM;

        ret = task_api_fill_file_info(file_info);
        if (ret) {
            kfree(file_info);
            return ret;
        }

        ret = user_copy_from_kernel(ptr, *file_info);

        kfree(file_info);
        return ret;
    }

    return -EINVAL;
}

struct procfs_entry_ops task_api_ops = {
    .read = scheduler_task_api_read,
    .ioctl = scheduler_task_api_ioctl,
};
