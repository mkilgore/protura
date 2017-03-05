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

static int socket_inode_delete(struct super_block *socket_superblock, struct inode *i)
{
    struct inode_socket *inode = container_of(i, struct inode_socket, i);

    socket_put(inode->socket);

    return 0;
}

static struct super_block_ops socket_fake_super_block_ops = {
    .inode_dealloc = socket_inode_dealloc,
    .inode_alloc = socket_inode_alloc,
    .inode_delete = socket_inode_delete,
};

static struct super_block socket_fake_super_block = SUPER_BLOCK_INIT(socket_fake_super_block);

static ino_t next_socket_ino = 1;

static struct inode_socket *new_socket_inode(void)
{
    struct inode *inode = socket_fake_super_block.ops->inode_alloc(&socket_fake_super_block);

    inode->ino = next_socket_ino++;
    inode->sb_dev = socket_fake_super_block.dev;
    inode->sb = &socket_fake_super_block;

    return container_of(inode, struct inode_socket, i);
}

int sys_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    struct file *filp;
    struct inode_socket *inode;
    struct socket *socket;
    int ret;

    ret = fd_get_checked(sockfd, &filp);
    if (ret)
        return ret;

    inode = container_of(filp->inode, struct inode_socket, i);
    socket = inode->socket;

    if (flag_test(&socket->flags, SOCKET_IS_BOUND))
        return -EINVAL;

    ret = socket->af->ops->bind(socket->af, socket, addr, addrlen);
    if (ret)
        return ret;

    ret = socket->proto->ops->bind(socket->proto, socket, addr, addrlen);
    if (ret)
        return ret;

    flag_set(&socket->flags, SOCKET_IS_BOUND);

    return 0;
}

int sys_getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    struct file *filp;
    struct inode_socket *inode;
    struct socket *socket;
    int ret;

    ret = fd_get_checked(sockfd, &filp);
    if (ret)
        return ret;

    inode = container_of(filp->inode, struct inode_socket, i);
    socket = inode->socket;

    if (!flag_test(&socket->flags, SOCKET_IS_BOUND))
        return -EINVAL;

    ret = socket->af->ops->getsockname(socket->af, socket, addr, addrlen);
    if (ret)
        return ret;

    ret = socket->proto->ops->getsockname(socket->proto, socket, addr, addrlen);
    if (ret)
        return ret;

    return 0;
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
    int ret;
    struct inode_socket *inode;
    struct socket *socket;
    struct packet *packet;

    kp(KP_NORMAL, "Sendto\n");

    inode = container_of(filp->inode, struct inode_socket, i);
    socket = inode->socket;

    packet = packet_new();

    packet_append_data(packet, buf, len);

    ret = socket->proto->ops->sendto(socket->proto, socket, packet, dest, addrlen);
    if (ret) {
        packet_free(packet);
        return ret;
    }

    ret = socket->af->ops->sendto(socket->af, socket, packet, dest, addrlen);
    if (ret) {
        packet_free(packet);
        return ret;
    }

    packet_linklayer_tx(packet);

    return 0;

}

int sys_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest, socklen_t addrlen)
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

int sys_send(int sockfd, const void *buf, size_t len, int flags)
{
    return sys_sendto(sockfd, buf, len, flags, NULL, 0);
}

int __sys_recvfrom(struct file *filp, void *buf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen)
{
    int ret = 0;
    struct inode_socket *inode;
    struct socket *socket;
    struct packet *packet = NULL;

    inode = container_of(filp->inode, struct inode_socket, i);
    socket = inode->socket;

    using_mutex(&socket->recv_lock) {
        if (list_empty(&socket->recv_queue)) {
            if (!flag_test(&filp->flags, FILE_NONBLOCK))
                ret = wait_queue_event_intr_mutex(&socket->recv_wait_queue, !list_empty(&socket->recv_queue), &socket->recv_lock);
            else
                ret = -EAGAIN;
        }

        if (!ret)
            packet = list_take_first(&socket->recv_queue, struct packet, packet_entry);

#if 0
      sleep_again:
        if (!list_empty(&socket->recv_queue)) {
            packet = list_take_first(&socket->recv_queue, struct packet, packet_entry);
        } else if (!flag_test(&filp->flags, FILE_NONBLOCK)) {
            ret = wait_queue_event_intr_mutex(&socket->recv_wait_queue, !list_empty(&socket->recv_queue), &socket->recv_lock);
            if (ret) {
                mutex_unlock(&socket->recv_lock);
                return ret;
            }
            goto sleep_again;
            sleep_intr_with_wait_queue(&socket->recv_wait_queue) {
                not_using_mutex(&socket->recv_lock) {
                    scheduler_task_yield();
                    if (has_pending_signal(cpu_get_local()->current)) {
                        wait_queue_unregister(&cpu_get_local()->current->wait);
                        return -ERESTARTSYS;
                    }
                }
                goto sleep_again;
            }
        }
#endif
    }

    if (ret)
        return ret;

    size_t plen = packet_len(packet);

    kp(KP_NORMAL, "Found packet, len:%d\n", plen);

    if (plen < len) {
        memcpy(buf, packet->head, plen);
        ret = plen;
    } else {
        memcpy(buf, packet->head, len);
        ret = len;
    }

    if (addr && *addrlen > packet->src_len) {
        memcpy(addr, &packet->src_addr, packet->src_len);
        *addrlen = packet->src_len;
    } else if (addr) {
        memcpy(addr, &packet->src_addr, *addrlen);
        *addrlen = packet->src_len;
    }

    packet_free(packet);

    return ret;

}

int sys_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen)
{
    struct file *filp;
    int ret;

    ret = fd_get_checked(sockfd, &filp);
    if (ret)
        return ret;

    return __sys_recvfrom(filp, buf, len, flags, addr, addrlen);
}

int sys_recv(int sockfd, void *buf, size_t len, int flags)
{
    return sys_recvfrom(sockfd, buf, len, flags, NULL, 0);
}

int sys_shutdown(int sockfd, int how)
{
    return -ENOTSUP;
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
    inode->socket = socket_alloc();

    inode->socket->address_family = afamily;
    inode->socket->sock_type = type;
    inode->socket->protocol = protocol;

    inode->socket->af = address_family_lookup(afamily);

    ret = (inode->socket->af->ops->create) (inode->socket->af, inode->socket);
    if (ret)
        goto release_socket;

    ret = (inode->socket->proto->ops->create) (inode->socket->proto, inode->socket);
    if (ret)
        goto release_socket;

    filp = kzalloc(sizeof(*filp), PAL_KERNEL);

    filp->mode = inode->i.mode;
    filp->inode = &inode->i;
    filp->flags = F(FILE_READABLE) | F(FILE_WRITABLE);
    filp->ops = &socket_file_ops;
    atomic_inc(&filp->ref);

    fd = fd_assign_empty(filp);

    if (fd == -1) {
        ret = -ENFILE;
        goto release_filp;
    }

    FD_CLR(fd, &current->close_on_exec);

    kp(KP_NORMAL, "Created socket: "PRinode"\n", Pinode(&inode->i));

    return fd;

    fd_release(fd);
  release_filp:
    kfree(filp);
  release_socket:
    socket_put(inode->socket);

    /* Inode currently has no references - create one and then drop it */
    inode_put(&inode->i);
    return ret;
}

void socket_subsystem_init(void)
{
    socket_fake_super_block.dev = block_dev_anon_get();
    socket_fake_super_block.bdev = 0;
    socket_fake_super_block.ops = &socket_fake_super_block_ops;
}

