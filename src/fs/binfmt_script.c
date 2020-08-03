/*
 * Copyright (C) 2016 Matt Kilgore
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
#include <protura/dump_mem.h>
#include <protura/mm/kmalloc.h>
#include <arch/task.h>
#include <arch/paging.h>
#include <arch/idt.h>

#include <protura/block/bcache.h>
#include <protura/fs/super.h>
#include <protura/fs/file.h>
#include <protura/fs/stat.h>
#include <protura/fs/inode.h>
#include <protura/fs/namei.h>
#include <protura/fs/sys.h>
#include <protura/fs/vfs.h>
#include <protura/fs/binfmt.h>

static int load_script(struct exe_params *params, struct irq_frame *frame)
{
    struct task *current = cpu_get_local()->current;
    struct file *old_exe;
    struct inode *script_ino;
    int ret;
    char head[256];
    char *exe;
    int exe_len;
    int head_len, i;

    head_len = vfs_read(params->exe, make_kernel_buffer(head), sizeof(head));

    exe_len = 0;
    for (i = 0; i < head_len; i++) {
        if (!head[i] || head[i] == '\n') {
            exe_len = i;
            break;
        }
    }

    exe_len -= 2;
    exe = head + 2;

    if (exe_len <= 0)
        return -ENOEXEC;

    exe[exe_len] = '\0';

    kp(KP_TRACE, "Script interpreter: %s\n", exe);

    ret = namex(exe, current->cwd, &script_ino);
    if (ret)
        return -ENOEXEC;

    old_exe = params->exe;

    ret = vfs_open(script_ino, F(FILE_READABLE), &params->exe);
    if (ret)
        goto cleanup_inode;

    ret = binary_load(params, frame);

    vfs_close(params->exe);
    params->exe = old_exe;

    if (ret)
        goto cleanup_inode;

    kp(KP_TRACE, "params->filename: %p\n", params->filename);
    kp(KP_TRACE, "exe: %p\n", exe);

    params_remove_args(params, 1);
    params_add_arg_first(params, params->filename);
    params_add_arg_first(params, exe);

  cleanup_inode:
    inode_put(script_ino);
    return 0;
}

static struct binfmt binfmt_script = BINFMT_INIT(binfmt_script, "script", "#!", load_script);

void script_register(void)
{
    binfmt_register(&binfmt_script);
}

void script_unregister(void)
{
    binfmt_unregister(&binfmt_script);
}

