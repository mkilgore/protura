/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/list.h>
#include <protura/mutex.h>
#include <protura/mm/kmalloc.h>

#include <arch/spinlock.h>
#include <protura/fs/block.h>
#include <protura/fs/char.h>
#include <protura/fs/stat.h>
#include <protura/fs/file.h>
#include <protura/fs/inode.h>
#include <protura/fs/inode_table.h>
#include <protura/fs/file_system.h>
#include <protura/fs/vfs.h>
#include <protura/fs/procfs.h>
#include "procfs_internal.h"

static int procfs_file_read(struct file *filp, void *buf, size_t size)
{
    struct procfs_inode *pinode = container_of(filp->inode, struct procfs_inode, i);
    struct procfs_node *node = pinode->node;
    struct procfs_entry *entry = container_of(node, struct procfs_entry, node);
    void *p;
    size_t data_len, cpysize;
    int ret;

    pinode->i.atime = protura_current_time_get();

    if (entry->read)
        return (entry->read)(filp, buf, size);

    if (filp->offset > 0)
        return 0;

    if (!entry->readpage)
        return 0;

    p = palloc_va(0, PAL_KERNEL);
    if (!p)
        return -ENOMEM;

    ret = (entry->readpage) (p, PG_SIZE, &data_len);

    kp(KP_TRACE, "procfs output len: %d\n", data_len);

    if (!ret) {
        cpysize = (data_len > size)? size: data_len;
        memcpy(buf, p, cpysize);
    }

    pfree_va(p, 0);


    if (ret) {
        return ret;
    } else {
        filp->offset = cpysize;
        return cpysize;
    }
}

struct file_ops procfs_file_file_ops = {
    .read = procfs_file_read,
};

struct inode_ops procfs_file_inode_ops = {
    /* We have nothing to implement */
};

