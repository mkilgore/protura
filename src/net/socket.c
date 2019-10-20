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
#include <arch/asm.h>

#include <protura/fs/procfs.h>
#include <protura/drivers/pci.h>
#include <protura/drivers/pci_ids.h>
#include <protura/net/socket.h>
#include <protura/net.h>

static mutex_t socket_list_lock = MUTEX_INIT(socket_list_lock, "socket-list-lock");
static atomic_t open_sockets = ATOMIC_INIT(0);
static list_head_t socket_list = LIST_HEAD_INIT(socket_list);

struct socket *socket_alloc(void)
{
    struct socket *socket;

    socket = kzalloc(sizeof(*socket), PAL_KERNEL);
    socket_init(socket);
    atomic_inc(&open_sockets);

    using_mutex(&socket_list_lock)
        list_add_tail(&socket_list, &socket->global_socket_entry);

    kp(KP_NORMAL, "Allocate socket: %p\n", socket);
    return socket_dup(socket);
}

void socket_free(struct socket *socket)
{
    atomic_dec(&open_sockets);

    using_mutex(&socket_list_lock)
        list_del(&socket->global_socket_entry);

    /* FIXME: Clear various queues and such */

    kp(KP_NORMAL, "Freeing socket: %p\n", socket);
    kfree(socket);
}

int socket_last_error(struct socket *socket)
{
    return xchg(&socket->last_err, 0);
}

void socket_set_last_error(struct socket *socket, int err)
{
    kp(KP_NORMAL, "Socket: Signalling last error: %d\n", err);
    xchg(&socket->last_err, err);
    wait_queue_wake(&socket->state_changed);
}

void socket_state_change(struct socket *socket, enum socket_state state)
{
    kp(KP_NORMAL, "Socket: Signalling state change to %d\n", state);
    xchg(&socket->state, state);
    wait_queue_wake(&socket->state_changed);
}

enum socket_state socket_state_cmpxchg(struct socket *socket, enum socket_state cur, enum socket_state new)
{
    kp(KP_NORMAL, "Socket: cmpxchg from %d to %d\n", cur, new);
    int ret = cmpxchg(&socket->state, cur, new);

    if (ret == new)
        wait_queue_wake(&socket->state_changed);

    return ret;
}

static const char *socket_state[] = {
    [SOCKET_UNCONNECTED] = "UNCONNECTED",
    [SOCKET_CONNECTING] = "CONNECTING",
    [SOCKET_CONNECTED] = "CONNECTED",
    [SOCKET_DISCONNECTING] = "DISCONNECTING",
};

static int socket_seq_start(struct seq_file *seq)
{
    mutex_lock(&socket_list_lock);
    return seq_list_start_header(seq, &socket_list);
}

static int socket_seq_render(struct seq_file *seq)
{
    struct socket *s = seq_list_get_entry(seq, struct socket, global_socket_entry);
    if (!s)
        return seq_printf(seq, "refs\taf\ttype\tproto\tstate\n");

    return seq_printf(seq, "%d\t%d\t%d\t%d\t%s\n",
            atomic_get(&s->refs),
            s->address_family,
            s->sock_type,
            s->protocol,
            socket_state[atomic_get(&s->state)]);
}

static int socket_seq_next(struct seq_file *seq)
{
    return seq_list_next(seq, &socket_list);
}

static void socket_seq_end(struct seq_file *seq)
{
    mutex_unlock(&socket_list_lock);
}

const static struct seq_file_ops socket_seq_file_ops = {
    .start = socket_seq_start,
    .next = socket_seq_next,
    .render = socket_seq_render,
    .end = socket_seq_end,
};

static int socket_file_seq_open(struct inode *inode, struct file *filp)
{
    return seq_open(filp, &socket_seq_file_ops);
}

struct file_ops socket_procfs_file_ops = {
    .open = socket_file_seq_open,
    .lseek = seq_lseek,
    .read = seq_read,
    .release = seq_release,
};
