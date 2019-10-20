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
#include <protura/net/ipv4/ip_route.h>
#include <protura/net/ipv4/ipv4.h>
#include <protura/net/linklayer.h>
#include <protura/net/socket.h>
#include <protura/net/sys.h>
#include <protura/net.h>

#include "sys_common.h"

static int socket_inode_dealloc(struct super_block *socket_superblock, struct inode *socket_inode)
{
    kfree(socket_inode);

    return 0;
}

static struct inode *socket_inode_alloc(struct super_block *socket_superblocK)
{
    struct inode_socket *inode = kzalloc(sizeof(*inode), PAL_KERNEL);

    inode_init(&inode->i);

    return &inode->i;
}

static struct super_block_ops socket_fake_super_block_ops = {
    .inode_dealloc = socket_inode_dealloc,
    .inode_alloc = socket_inode_alloc,
};

static struct super_block socket_fake_super_block = SUPER_BLOCK_INIT(socket_fake_super_block);

static ino_t next_socket_ino = 1;

static struct inode_socket *new_socket_inode(void)
{
    struct inode *inode = socket_fake_super_block.ops->inode_alloc(&socket_fake_super_block);

    inode->ino = next_socket_ino++;
    inode->sb_dev = socket_fake_super_block.dev;
    inode->sb = &socket_fake_super_block;
    inode->mode = S_IFSOCK;

    return container_of(inode, struct inode_socket, i);
}

static int fd_get_socket(int sockfd, struct socket **sock)
{
    int ret;
    struct file *filp;
    struct inode_socket *inode;

    ret = fd_get_checked(sockfd, &filp);
    if (ret)
        return ret;

    if (!S_ISSOCK(filp->inode->mode))
        return -ENOTSOCK;

    inode = container_of(filp->inode, struct inode_socket, i);
    *sock = inode->socket;

    return 0;
}

int sys_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    struct socket *socket;
    int ret;

    ret = user_check_region(addr, addrlen, F(VM_MAP_READ));
    if (ret)
        return ret;

    ret = fd_get_socket(sockfd, &socket);
    if (ret)
        return ret;

    return socket_bind(socket, addr, addrlen);
}

int sys_getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    struct socket *socket;
    int ret;

    ret = user_check_region(addrlen, sizeof(*addrlen), F(VM_MAP_READ));
    if (ret)
        return ret;

    ret = user_check_region(addr, *addrlen, F(VM_MAP_READ));
    if (ret)
        return ret;

    ret = fd_get_socket(sockfd, &socket);
    if (ret)
        return ret;

    return socket_getsockname(socket, addr, addrlen);
}

int sys_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
    struct socket *socket;
    int ret;

    ret = user_check_region(optval, optlen, F(VM_MAP_READ));
    if (ret)
        return ret;

    ret = fd_get_socket(sockfd, &socket);
    if (ret)
        return ret;

    return socket_setsockopt(socket, level, optname, optval, optlen);
}

int sys_getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
    struct socket *socket;
    int ret;

    ret = user_check_region(optlen, sizeof(*optlen), F(VM_MAP_READ) | F(VM_MAP_WRITE));
    if (ret)
        return ret;

    ret = user_check_region(optval, *optlen, F(VM_MAP_READ) | F(VM_MAP_WRITE));
    if (ret)
        return ret;

    ret = fd_get_socket(sockfd, &socket);
    if (ret)
        return ret;

    return socket_getsockopt(socket, level, optname, optval, optlen);
}

int __sys_sendto(struct file *filp, const void *buf, size_t len, int flags, const struct sockaddr *dest, socklen_t addrlen)
{
    struct inode_socket *inode;
    struct socket *socket;

    if (!S_ISSOCK(filp->inode->mode))
        return -ENOTSOCK;

    inode = container_of(filp->inode, struct inode_socket, i);
    socket = inode->socket;

    return socket_sendto(socket, buf, len, flags, dest, addrlen, flag_test(&filp->flags, FILE_NONBLOCK));
}

int sys_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest, socklen_t addrlen)
{
    struct file *filp;
    int ret;

    kp(KP_NORMAL, "dest: %p, len: %d\n", dest, addrlen);

    ret = user_check_region(buf, len, F(VM_MAP_READ) | F(VM_MAP_WRITE));
    if (ret)
        return ret;

    if (dest) {
        ret = user_check_region(dest, addrlen, F(VM_MAP_READ));
        if (ret)
            return ret;
    }

    if (len > 1500)
        return -EMSGSIZE;

    ret = fd_get_checked(sockfd, &filp);
    if (ret)
        return ret;

    return __sys_sendto(filp, buf, len, flags, dest, addrlen);
}

int sys_send(int sockfd, const void *buf, size_t len, int flags)
{
    return sys_sendto(sockfd, buf, len, flags, NULL, 0);
}

int __sys_recvfrom(struct file *filp, void *buf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen)
{
    struct inode_socket *inode;
    struct socket *socket;

    if (!S_ISSOCK(filp->inode->mode))
        return -ENOTSOCK;

    inode = container_of(filp->inode, struct inode_socket, i);
    socket = inode->socket;

    return socket_recvfrom(socket, buf, len, flags, addr, addrlen, flag_test(&filp->flags, FILE_NONBLOCK));
}

int sys_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen)
{
    struct file *filp;
    int ret;

    kp(KP_NORMAL, "sys_recvfrom: addrlen: %p, *addrlen: %d\n", addrlen, addrlen? *addrlen: 100);

    ret = user_check_region(buf, len, F(VM_MAP_READ) | F(VM_MAP_WRITE));
    if (ret)
        return ret;

    if (addr && addrlen) {
        ret = user_check_region(addrlen, sizeof(*addrlen), F(VM_MAP_READ));
        if (ret)
            return ret;

        ret = user_check_region(addr, *addrlen, F(VM_MAP_READ));
        if (ret)
            return ret;
    } else {
        addr = NULL;
        addrlen = NULL;
    }

    ret = fd_get_checked(sockfd, &filp);
    if (ret)
        return ret;

    return __sys_recvfrom(filp, buf, len, flags, addr, addrlen);
}

int sys_recv(int sockfd, void *buf, size_t len, int flags)
{
    return sys_recvfrom(sockfd, buf, len, flags, NULL, NULL);
}

int sys_shutdown(int sockfd, int how)
{
    struct socket *socket;
    int ret;

    ret = fd_get_socket(sockfd, &socket);
    if (ret)
        return ret;

    return socket_shutdown(socket, how);
}

int sys_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    struct socket *socket, *new_socket = NULL;
    int ret;

    if (addr && addrlen) {
        ret = user_check_region(addrlen, sizeof(*addrlen), F(VM_MAP_READ));
        if (ret)
            return ret;

        ret = user_check_region(addr, *addrlen, F(VM_MAP_READ));
        if (ret)
            return ret;
    } else {
        addr = NULL;
        addrlen = NULL;
    }

    ret = fd_get_socket(sockfd, &socket);
    if (ret)
        return ret;

    ret = socket_accept(socket, addr, addrlen, &new_socket);
    if (ret)
        return ret;

    /* FIXME: Create a new struct file and fill it in with the created socket */
    return 0;
}

int sys_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    struct socket *socket;
    int ret;

    if (addr && addrlen) {
        ret = user_check_region(addr, addrlen, F(VM_MAP_READ));
        if (ret)
            return ret;
    } else {
        addr = NULL;
        addrlen = 0;
    }

    ret = fd_get_socket(sockfd, &socket);
    if (ret)
        return ret;

    return socket_connect(socket, addr, addrlen);
}

int sys_listen(int sockfd, int backlog)
{
    struct socket *socket;
    int ret;

    ret = fd_get_socket(sockfd, &socket);
    if (ret)
        return ret;

    return socket_listen(socket, backlog);
}

int sys_socket(int afamily, int type, int protocol)
{
    int ret = 0;
    int fd;
    struct file *filp;
    struct inode_socket *inode;
    struct task *current = cpu_get_local()->current;

    inode = new_socket_inode();

    if (!inode)
        return -ENFILE;

    inode_dup(&inode->i);

    /* We initalize the socket first */
    ret = socket_open(afamily, type & SOCK_MASK, protocol, &inode->socket);
    if (ret)
        goto release_inode;

    filp = kzalloc(sizeof(*filp), PAL_KERNEL);

    filp->mode = inode->i.mode;
    filp->inode = &inode->i;
    filp->flags = F(FILE_READABLE) | F(FILE_WRITABLE);
    filp->ops = &socket_file_ops;
    atomic_inc(&filp->ref);

    if (type & SOCK_NONBLOCK)
        flag_set(&filp->flags, FILE_NONBLOCK);

    fd = fd_assign_empty(filp);

    if (fd == -1) {
        ret = -ENFILE;
        goto release_filp;
    }

    if (type & SOCK_CLOEXEC)
        FD_SET(fd, &current->close_on_exec);
    else
        FD_CLR(fd, &current->close_on_exec);

    kp(KP_NORMAL, "Created socket: "PRinode"\n", Pinode(&inode->i));

    return fd;

    fd_release(fd);
  release_filp:
    kfree(filp);
    socket_put(inode->socket);

  release_inode:
    inode_put(&inode->i);
    return ret;
}

void socket_subsystem_init(void)
{
    socket_fake_super_block.dev = block_dev_anon_get();
    socket_fake_super_block.bdev = 0;
    socket_fake_super_block.ops = &socket_fake_super_block_ops;
}

