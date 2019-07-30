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
#include <protura/net/ipv4/ip_route.h>
#include <protura/net/ipv4/ipv4.h>
#include <protura/net/linklayer.h>
#include <protura/net/socket.h>
#include <protura/net/sys.h>
#include <protura/net.h>

int socket_open(int domain, int type, int protocol, struct socket **sock_ret)
{
    struct socket *socket;
    int ret;

    socket = socket_alloc();

    socket->address_family = domain;
    socket->sock_type = type;
    socket->protocol = protocol;

    socket->af = address_family_lookup(domain);

    ret = (socket->af->ops->create) (socket->af, socket);
    if (ret)
        goto release_socket;

    if (socket->proto) {
        ret = (socket->proto->ops->create) (socket->proto, socket);
        if (ret)
            goto release_socket;
    }

    *sock_ret = socket;
    return ret;

  release_socket:
    socket_put(socket);
    return ret;
}

int socket_sendto(struct socket *socket, const void *buf, size_t len, int flags, const struct sockaddr *dest, socklen_t addrlen, int nonblock)
{
    int ret;
    struct packet *packet;

    packet = packet_new();

    packet_append_data(packet, buf, len);

    if (socket->proto) {
        kp(KP_NORMAL, "Socket: %p\n", socket);
        kp(KP_NORMAL, "proto: %p\n", socket->proto);
        kp(KP_NORMAL, "ops: %p\n", socket->proto->ops);
        kp(KP_NORMAL, "sendto: %p\n", socket->proto->ops->sendto);
        ret = socket->proto->ops->sendto(socket->proto, socket, packet, dest, addrlen);
        if (ret) {
            packet_free(packet);
            return ret;
        }
    } else {
        kp(KP_NORMAL, "NO PROTO Socket: %p\n", socket);
        kp(KP_NORMAL, "af: %p\n", socket->af);
        kp(KP_NORMAL, "ops: %p\n", socket->af->ops);
        kp(KP_NORMAL, "sendto: %p\n", socket->af->ops->sendto);
        packet->protocol_type = socket->protocol;

        ret = socket->af->ops->sendto(socket->af, socket, packet, dest, addrlen);
        if (ret) {
            packet_free(packet);
            return ret;
        }
    }

    return 0;
}

int socket_send(struct socket *socket, const void *buf, size_t len, int flags)
{
    return socket_sendto(socket, buf, len, flags, NULL, 0, 0);
}

int socket_recvfrom(struct socket *socket, void *buf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen, int nonblock)
{
    int ret = 0;
    struct packet *packet = NULL;

    kp(KP_NORMAL, "nonblock: %d\n", nonblock);

    using_mutex(&socket->recv_lock) {
        if (list_empty(&socket->recv_queue)) {
            if (!nonblock)
                ret = wait_queue_event_intr_mutex(&socket->recv_wait_queue, !list_empty(&socket->recv_queue), &socket->recv_lock);
            else
                ret = -EAGAIN;
        }

        if (!ret)
            packet = list_take_first(&socket->recv_queue, struct packet, packet_entry);
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

int socket_recv(struct socket *socket, void *buf, size_t len, int flags)
{
    return socket_recvfrom(socket, buf, len, flags, NULL, NULL, 0);
}

int socket_bind(struct socket *socket, const struct sockaddr *addr, socklen_t addrlen)
{
    int ret;

    if (flag_test(&socket->flags, SOCKET_IS_BOUND))
        return -EINVAL;

    ret = socket->af->ops->bind(socket->af, socket, addr, addrlen);
    if (ret)
        return ret;

    if (socket->proto) {
        ret = socket->proto->ops->bind(socket->proto, socket, addr, addrlen);
        if (ret)
            return ret;
    }

    flag_set(&socket->flags, SOCKET_IS_BOUND);

    return 0;
}

int socket_getsockname(struct socket *socket, struct sockaddr *addr, socklen_t *addrlen)
{
    int ret;

    if (!flag_test(&socket->flags, SOCKET_IS_BOUND))
        return -EINVAL;

    ret = socket->af->ops->getsockname(socket->af, socket, addr, addrlen);
    if (ret)
        return ret;

    if (socket->proto) {
        ret = socket->proto->ops->getsockname(socket->proto, socket, addr, addrlen);
        if (ret)
            return ret;
    }

    return 0;
}

int socket_setsockopt(struct socket *socket, int level, int optname, const void *optval, socklen_t optlen)
{
    return -ENOTSUP;
}

int socket_getsockopt(struct socket *socket, int level, int optname, void *optval, socklen_t *optlen)
{
    return -ENOTSUP;
}

int socket_accept(struct socket *socket, struct sockaddr *addr, socklen_t *addrlen)
{
    return -ENOTSUP;
}

int socket_connect(struct socket *socket, const struct sockaddr *addr, socklen_t addrlen)
{
    return -ENOTSUP;
}

int socket_listen(struct socket *socket, int backlog)
{
    return -ENOTSUP;
}

int socket_shutdown(struct socket *socket, int how)
{
    if (flag_test(&socket->flags, SOCKET_IS_CLOSED))
        return 0;

    (socket->af->ops->delete) (socket->af, socket);
    if (socket->proto)
        (socket->proto->ops->delete) (socket->proto, socket);

    flag_set(&socket->flags, SOCKET_IS_CLOSED);

    return 0;
}

