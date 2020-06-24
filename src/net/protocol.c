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
#include <protura/fs/seq_file.h>
#include <protura/fs/vfs.h>
#include <protura/net/socket.h>
#include <protura/net/sys.h>
#include <protura/net.h>

static mutex_t proto_list_lock = MUTEX_INIT(proto_list_lock);
static list_head_t proto_list = LIST_HEAD_INIT(proto_list);

struct proto_state {
    struct protocol *proto;
    list_node_t *cur_sock;
};

int proto_seq_start(struct seq_file *seq, struct protocol *proto)
{
    struct proto_state *state = kmalloc(sizeof(*state), PAL_KERNEL);

    mutex_lock(&proto->lock);

    state->proto = proto;
    state->cur_sock = seq_list_start_header_node(seq, &proto->socket_list);

    seq->priv = state;
    return 0;
}

struct socket *proto_seq_get_socket(struct seq_file *seq)
{
    struct proto_state *state = seq->priv;

    if (!state->cur_sock)
        return NULL;

    return container_of(state->cur_sock, struct socket, proto_entry);
}

int proto_seq_next(struct seq_file *seq)
{
    struct proto_state *state = seq->priv;

    state->cur_sock = seq_list_next_node(seq, state->cur_sock, &state->proto->socket_list);
    return 0;
}

void proto_seq_end(struct seq_file *seq)
{
    struct proto_state *state = seq->priv;

    mutex_unlock(&state->proto->lock);
    kfree(state);
}
