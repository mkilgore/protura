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
#include <protura/kassert.h>
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

static mutex_t proto_list_lock = MUTEX_INIT(proto_list_lock, "proto-list-lock");
static list_head_t proto_list = LIST_HEAD_INIT(proto_list);

void protocol_register(struct protocol *proto)
{
    using_mutex(&proto_list_lock) {
        list_add(&proto_list, &proto->proto_entry);
    }
}

struct protocol *protocol_lookup(enum protocol_type p)
{
    struct protocol *proto, *ret = NULL;
    using_mutex(&proto_list_lock) {
        list_foreach_entry(&proto_list, proto, proto_entry) {
            if (proto->protocol_id == p) {
                ret = proto;
                break;
            }
        }
    }

    return ret;
}

