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
#include <protura/net/af/ip_route.h>
#include <protura/net/af/ipv4.h>
#include <protura/net/linklayer.h>
#include <protura/net/socket.h>
#include <protura/net/sys.h>
#include <protura/net.h>

struct inode_socket {
    struct inode i;
    struct socket *socket;
};

static int socket_poll(struct file *filp, struct poll_table *table, int events)
{
    struct inode_socket *inode;
    struct socket *socket;
    int ret = 0;

    inode = container_of(filp->inode, struct inode_socket, i);
    socket = inode->socket;

    if (events & POLLOUT)
        ret |= POLLOUT;

    using_mutex(&socket->recv_lock) {
        if (events & POLLIN) {
            if (!list_empty(&socket->recv_queue))
                ret |= POLLIN;
            else
                poll_table_add(table, &socket->recv_wait_queue);
        }
    }

    return ret;
}

static int socket_release(struct file *filp)
{
    struct inode_socket *inode;
    struct socket *socket;

    inode = container_of(filp->inode, struct inode_socket, i);
    socket = inode->socket;

    if (flag_test(&socket->flags, SOCKET_IS_CLOSED))
        return 0;

    (socket->af->ops->delete) (socket->af, socket);
    (socket->proto->ops->delete) (socket->proto, socket);

    flag_set(&socket->flags, SOCKET_IS_CLOSED);

    return 0;
}

static int socket_read(struct file *filp, void *vptr, size_t len)
{
    return __sys_recvfrom(filp, vptr, len, 0, NULL, NULL);
}

static int socket_write(struct file *filp, const void *vptr, size_t len)
{
    return __sys_sendto(filp, vptr, len, 0, NULL, 0);
}

static int socket_ioctl(struct file *filp, int cmd, uintptr_t arg)
{
    return -EINVAL;
}

struct file_ops socket_file_ops = {
    .poll = socket_poll,
    .release = socket_release,

    .read = socket_read,
    .write = socket_write,
    .ioctl = socket_ioctl,
};

