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
#include <protura/rwlock.h>
#include <protura/atomic.h>
#include <protura/mm/kmalloc.h>
#include <arch/task.h>

#include <protura/fs/block.h>
#include <protura/fs/super.h>
#include <protura/fs/file.h>
#include <protura/fs/vfs.h>
#include <protura/fs/binfmt.h>

static struct {
    rwlock_t lock;

    list_head_t list;
} binfmt_table = {
    .lock = RWLOCK_INIT(binfmt_table.lock, "Binfmt table lock"),
    .list = LIST_HEAD_INIT(binfmt_table.list),
};

void binfmt_register(struct binfmt *fmt)
{
    using_rwlock_w(&binfmt_table.lock)
        list_add_tail(&binfmt_table.list, &fmt->binfmt_list_entry);
}

void binfmt_unregister(struct binfmt *fmt)
{
    using_rwlock_w(&binfmt_table.lock) {
        if (atomic_get(&fmt->use_count) == 0)
            list_del(&fmt->binfmt_list_entry);
        else
            panic("Attempting to remove a binfmt while in use!\n");
    }
}

int binary_load(struct exe_params *params, struct irq_frame *frame)
{
    int ret;

    using_rwlock_r(&binfmt_table.lock) {
        struct binfmt *fmt;
        list_foreach_entry(&binfmt_table.list, fmt, binfmt_list_entry) {
            kp(KP_TRACE, "Bin format: %s\n", fmt->name);
            ret = (fmt->load_bin) (params, frame);
            if (ret)
                break;
        }
    }

    return 0;
}

