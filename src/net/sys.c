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
#include <protura/mm/user_check.h>
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

static int socket_inode_dealloc(struct super_block *socket_superblock, struct inode *inode)
{
    struct inode_socket *inode_socket = container_of(inode, struct inode_socket, i);
    kfree(inode_socket);

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

static int user_read_sockaddr(struct sockaddr *addr, struct user_buffer user, socklen_t len)
{
    if (len < 0 || sizeof(*addr) < len)
        return -EINVAL;

    if (len == 0)
        return 0;

    return user_memcpy_to_kernel(addr, user, len);

}

static int user_write_sockaddr(struct sockaddr *addr, socklen_t len, struct user_buffer user, struct user_buffer user_socklen)
{
    socklen_t user_len;

    int ret = user_copy_to_kernel(&user_len, user_socklen);
    if (ret)
        return ret;

    if (user_len < 0)
        return -EINVAL;

    if (user_len > len)
        user_len = len;

    if (user_len) {
        ret = user_memcpy_from_kernel(user, &addr, user_len);
        if (ret)
            return ret;
    }

    return user_copy_from_kernel(user_socklen, len);
}

int sys_bind(int sockfd, struct user_buffer addr, socklen_t addrlen)
{
    struct sockaddr cpy;
    struct socket *socket;
    int ret;

    ret = user_read_sockaddr(&cpy, addr, addrlen);
    if (ret)
        return ret;

    ret = fd_get_socket(sockfd, &socket);
    if (ret)
        return ret;

    return socket_bind(socket, &cpy, addrlen);
}

int sys_getsockname(int sockfd, struct user_buffer user_addr, struct user_buffer user_addrlen)
{
    struct sockaddr cpy;
    socklen_t len = sizeof(cpy);
    struct socket *socket;
    int ret;

    ret = fd_get_socket(sockfd, &socket);
    if (ret)
        return ret;

    ret = socket_getsockname(socket, &cpy, &len);
    if (ret)
        return ret;

    return user_write_sockaddr(&cpy, len, user_addr, user_addrlen);
}

int sys_setsockopt(int sockfd, int level, int optname, struct user_buffer optval, socklen_t optlen)
{
    struct socket *socket;
    int ret;

    ret = fd_get_socket(sockfd, &socket);
    if (ret)
        return ret;

    return socket_setsockopt(socket, level, optname, optval, optlen);
}

int sys_getsockopt(int sockfd, int level, int optname, struct user_buffer optval, struct user_buffer optlen)
{
    struct socket *socket;
    int ret;

    ret = fd_get_socket(sockfd, &socket);
    if (ret)
        return ret;

    return socket_getsockopt(socket, level, optname, optval, optlen);
}

int __sys_sendto(struct file *filp, struct user_buffer buf, size_t buflen, int flags, struct user_buffer dest, socklen_t addrlen)
{
    struct sockaddr cpy;
    struct inode_socket *inode;
    struct socket *socket;

    if (!S_ISSOCK(filp->inode->mode))
        return -ENOTSOCK;

    int ret = user_read_sockaddr(&cpy, dest, addrlen);
    if (ret)
        return ret;

    inode = container_of(filp->inode, struct inode_socket, i);
    socket = inode->socket;

    struct sockaddr *sockbuf = &cpy;
    if (!addrlen)
        sockbuf = NULL;

    /* FIXME: This should take a user_buffer */
    return socket_sendto(socket, buf, buflen, flags, sockbuf, addrlen, flag_test(&filp->flags, FILE_NONBLOCK));
}

int sys_sendto(int sockfd, struct user_buffer buf, size_t len, int flags, struct user_buffer dest, socklen_t addrlen)
{
    struct file *filp;
    int ret;

    if (len > 1500)
        return -EMSGSIZE;

    ret = fd_get_checked(sockfd, &filp);
    if (ret)
        return ret;

    return __sys_sendto(filp, buf, len, flags, dest, addrlen);
}

int sys_send(int sockfd, struct user_buffer buf, size_t len, int flags)
{
    return sys_sendto(sockfd, buf, len, flags, make_user_buffer(NULL), 0);
}

int __sys_recvfrom(struct file *filp, struct user_buffer buf, size_t len, int flags, struct user_buffer addr, struct user_buffer addrlen)
{
    struct sockaddr cpy;
    socklen_t cpylen = sizeof(cpy);
    struct inode_socket *inode;
    struct socket *socket;

    if (!S_ISSOCK(filp->inode->mode))
        return -ENOTSOCK;

    inode = container_of(filp->inode, struct inode_socket, i);
    socket = inode->socket;

    /* FIXME: This should take a user_buffer */
    int ret = socket_recvfrom(socket, buf, len, flags, &cpy, &cpylen, flag_test(&filp->flags, FILE_NONBLOCK));
    if (ret < 0)
        return ret;

    if (addr.ptr && addrlen.ptr) {
        int err = user_write_sockaddr(&cpy, cpylen, addr, addrlen);
        if (err)
            return err;
    }

    return ret;
}

int sys_recvfrom(int sockfd, struct user_buffer buf, size_t len, int flags, struct user_buffer addr, struct user_buffer addrlen)
{
    struct file *filp;
    int ret;

    ret = fd_get_checked(sockfd, &filp);
    if (ret)
        return ret;

    return __sys_recvfrom(filp, buf, len, flags, addr, addrlen);
}

int sys_recv(int sockfd, struct user_buffer buf, size_t len, int flags)
{
    return sys_recvfrom(sockfd, buf, len, flags, make_user_buffer(NULL), make_user_buffer(NULL));
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

int sys_accept(int sockfd, struct user_buffer addr, struct user_buffer addrlen)
{
    struct sockaddr cpy;
    socklen_t cpylen = sizeof(cpy);
    struct socket *socket, *new_socket = NULL;
    int ret;

    ret = fd_get_socket(sockfd, &socket);
    if (ret)
        return ret;

    ret = socket_accept(socket, &cpy, &cpylen, &new_socket);
    if (ret)
        return ret;

    if (addr.ptr && addrlen.ptr) {
        /* FIXME: Don't just leak the FD...*/
        int err = user_write_sockaddr(&cpy, cpylen, addr, addrlen);
        if (err)
            return err;
    }

    /* FIXME: Create a new struct file and fill it in with the created socket */
    return 0;
}

int sys_connect(int sockfd, struct user_buffer addr, socklen_t addrlen)
{
    struct sockaddr cpy;
    struct sockaddr *ptr = &cpy;
    struct socket *socket;
    int ret;

    if (addr.ptr && addrlen) {
        ret = user_read_sockaddr(&cpy, addr, addrlen);
        if (ret)
            return ret;
    } else {
        ptr = NULL;
        addrlen = 0;
    }

    ret = fd_get_socket(sockfd, &socket);
    if (ret)
        return ret;

    return socket_connect(socket, ptr, addrlen);
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

