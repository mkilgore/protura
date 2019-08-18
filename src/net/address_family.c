/*
 * Copyright (C) 2017 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/mm/kmalloc.h>
#include <protura/snprintf.h>
#include <protura/list.h>

#include <protura/fs/file.h>
#include <protura/fs/stat.h>
#include <protura/fs/inode.h>
#include <protura/fs/inode_table.h>
#include <protura/fs/vfs.h>
#include <protura/net/socket.h>
#include <protura/net/sys.h>
#include <protura/net.h>

static mutex_t af_list_lock = MUTEX_INIT(af_list_lock, "af-list-lock");
static list_head_t af_list = LIST_HEAD_INIT(af_list);

void address_family_register(struct address_family *af)
{
    using_mutex(&af_list_lock) {
        list_add(&af_list, &af->af_entry);
    }
}

struct address_family *address_family_lookup(int af)
{
    struct address_family *afamily, *ret = NULL;
    using_mutex(&af_list_lock) {
        list_foreach_entry(&af_list, afamily, af_entry) {
            if (afamily->af_type == af) {
                ret = afamily;
                break;
            }
        }
    }

    return ret;
}

void address_family_setup(void)
{
    struct address_family *afamily;
    using_mutex(&af_list_lock)
        list_foreach_entry(&af_list, afamily, af_entry)
            not_using_mutex(&af_list_lock)
                (afamily->ops->setup_af) (afamily);
}

