/*
 * Copyright (C) 2018 Matt Kilgore
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
#include <protura/fs/vfs.h>
#include <protura/net/ipv4/ip_route.h>
#include <protura/net/ipv4/ipv4.h>
#include <protura/net/linklayer.h>
#include <protura/net/socket.h>
#include <protura/net/sys.h>
#include <protura/net.h>

#include "sys_common.h"

/* This file is used when net support is diabled. It provides stubs for the
 * syscalls normally handled by the net subsystem */

int sys_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return -ENOTSUP;
}

int sys_getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    return -ENOTSUP;
}

int sys_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
    return -ENOTSUP;
}

int sys_getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
    return -ENOTSUP;
}

int __sys_sendto(struct file *filp, const void *buf, size_t len, int flags, const struct sockaddr *dest, socklen_t addrlen)
{
    return -ENOTSUP;
}

int sys_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest, socklen_t addrlen)
{
    return -ENOTSUP;
}

int sys_send(int sockfd, const void *buf, size_t len, int flags)
{
    return -ENOTSUP;
}

int __sys_recvfrom(struct file *filp, void *buf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen)
{
    return -ENOTSUP;
}

int sys_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen)
{
    return -ENOTSUP;
}

int sys_recv(int sockfd, void *buf, size_t len, int flags)
{
    return -ENOTSUP;
}

int sys_shutdown(int sockfd, int how)
{
    return -ENOTSUP;
}

int sys_socket(int afamily, int type, int protocol)
{
    return -ENOTSUP;
}

