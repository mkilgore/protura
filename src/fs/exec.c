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
#include <arch/spinlock.h>
#include <protura/mutex.h>
#include <protura/atomic.h>
#include <mm/kmalloc.h>
#include <arch/task.h>

#include <fs/block.h>
#include <fs/super.h>
#include <fs/file.h>
#include <fs/stat.h>
#include <fs/inode.h>
#include <fs/namei.h>
#include <fs/sys.h>
#include <fs/vfs.h>
#include <fs/binfmt.h>
#include <fs/exec.h>

int exec(struct inode *inode, char *const argv[], struct irq_frame *frame)
{
    struct file *filp;
    struct exe_params params;
    int fd;
    int ret;

    fd = __sys_open(inode, F(FILE_READABLE), &filp);
    if (fd < 0)
        return fd;

    params.exe = filp;

    ret = binary_load(&params, frame);

    sys_close(fd);

    return ret;
}

int sys_exec(const char *file, char *const argv[], struct irq_frame *frame)
{
    struct inode *exe;
    struct task *current = cpu_get_local()->current;
    int ret;

    kprintf("EXEC file: %p, argv: %p, frame: %p\n", file, argv, frame);

    ret = namex(file, current->cwd, &exe);
    if (ret)
        return ret;

    exec(exe, argv, frame);

    inode_put(exe);

    return ret;
}

