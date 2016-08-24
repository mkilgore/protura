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
#include <protura/snprintf.h>
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
    mutex_t lock;

    list_head_t list;
} binfmt_table = {
    .lock = MUTEX_INIT(binfmt_table.lock, "Binfmt table lock"),
    .list = LIST_HEAD_INIT(binfmt_table.list),
};

void binfmt_register(struct binfmt *fmt)
{
    using_mutex(&binfmt_table.lock) {
        list_add(&binfmt_table.list, &fmt->binfmt_list_entry);
        kp(KP_TRACE, "Added binfmt: %s\n", fmt->name);
    }
}

void binfmt_unregister(struct binfmt *fmt)
{
    using_mutex(&binfmt_table.lock) {
        if (atomic_get(&fmt->use_count) == 0)
            list_del(&fmt->binfmt_list_entry);
        else
            panic("Attempting to remove a binfmt while in use!\n");
    }
}

int binary_load(struct exe_params *params, struct irq_frame *frame)
{
    int ret = -ENOEXEC;
    char begin[128];

    memset(begin, 0, sizeof(begin));
    vfs_read(params->exe, begin, sizeof(begin));
    vfs_lseek(params->exe, 0, SEEK_SET);

    using_mutex(&binfmt_table.lock) {
        struct binfmt *fmt;
        list_foreach_entry(&binfmt_table.list, fmt, binfmt_list_entry) {
            kp(KP_TRACE, "Checking format: %s\n", fmt->name);
            if (strncmp(begin, fmt->magic, strlen(fmt->magic)) != 0) {
                kp(KP_TRACE, "Skipping format: %s\n", fmt->name);
                ret = -ENOEXEC;
                continue;
            }

            kp(KP_TRACE, "Bin format: %s\n", fmt->name);
            not_using_mutex(&binfmt_table.lock)
                ret = (fmt->load_bin) (params, frame);

            break;
        }
        kp(KP_TRACE, "binfmt done.\n");
    }

    return ret;
}

int binfmt_readpage(void *page, size_t page_size, size_t *len)
{
    struct binfmt *fmt;

    *len = 0;

    using_mutex(&binfmt_table.lock)
        list_foreach_entry(&binfmt_table.list, fmt, binfmt_list_entry) {
            *len += snprintf(page + *len, page_size - *len, "%s\n", fmt->name);
            kp(KP_TRACE, "fmt: %s\n", fmt->name);
        }

    return 0;
}

