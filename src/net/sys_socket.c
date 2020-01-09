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

int socket_open(int domain, int type, int protocol, struct socket **sock_ret)
{
    struct socket *socket;
    int ret;

    socket = socket_alloc();

    kp(KP_NORMAL, "Socket alloc: %p, domain: %d, type: %d, proto: %d\n", socket, domain, type, protocol);
    socket->address_family = domain;
    socket->sock_type = type;
    socket->protocol = protocol;
    socket->af = address_family_lookup(domain);
    kp(KP_NORMAL, "Socket AF: %p\n", socket->af);

    ret = (socket->af->ops->create) (socket->af, socket);
    if (ret || !socket->proto)
        goto release_socket;

    if (socket->proto->ops->create) {
        ret = (socket->proto->ops->create) (socket->proto, socket);
        if (ret)
            goto release_socket;
    }

    kp(KP_NORMAL, "Socket ret: %p, refs: %d\n", socket, atomic_get(&socket->refs));
    *sock_ret = socket;
    return ret;

  release_socket:
    socket_put(socket);
    return ret;
}

int socket_sendto(struct socket *socket, struct user_buffer buf, size_t len, int flags, const struct sockaddr *dest, socklen_t addrlen, int nonblock)
{
    kp(KP_NORMAL, "Socket: %p, socklen: %d, dest: %p\n", socket, addrlen, dest);
    kp(KP_NORMAL, "proto: %p\n", socket->proto);
    kp(KP_NORMAL, "ops: %p\n", socket->proto->ops);
    kp(KP_NORMAL, "sendto: %p\n", socket->proto->ops->sendto);

    if (socket->proto->ops->sendto)
        return socket->proto->ops->sendto(socket->proto, socket, buf, len, dest, addrlen);
    else
        return -ENOTSUP;
}

int socket_send(struct socket *socket, struct user_buffer buf, size_t len, int flags)
{
    return socket_sendto(socket, buf, len, flags, NULL, 0, 0);
}

int socket_recvfrom(struct socket *socket, struct user_buffer buf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen, int nonblock)
{
    int ret = 0;
    struct packet *packet = NULL;

    using_mutex(&socket->recv_lock) {
        if (list_empty(&socket->recv_queue)) {
            if (!nonblock)
                ret = wait_queue_event_intr_mutex(&socket->recv_wait_queue, !list_empty(&socket->recv_queue), &socket->recv_lock);
            else
                ret = -EAGAIN;
        }

        if (!ret) {
            /* We may request less data than is in this packet. In that case,
             * we remove that data from the packet but keep it in the queue */
            packet = list_first(&socket->recv_queue, struct packet, packet_entry);

            size_t plen = packet_len(packet);
            int drop_packet = 0;

            if (plen <= len) {
                ret = user_memcpy_from_kernel(buf, packet->head, plen);
                if (ret)
                    return ret;

                ret = plen;
                drop_packet = 1;
            } else {
                ret = user_memcpy_from_kernel(buf, packet->head, len);
                if (ret)
                    return ret;

                ret = len;

                /* move the head past the read data */
                packet->head += len;
            }

            if (addr && *addrlen >= packet->src_len) {
                memcpy(addr, &packet->src_addr, packet->src_len);
                *addrlen = packet->src_len;
            } else if (addr) {
                memcpy(addr, &packet->src_addr, *addrlen);
                *addrlen = packet->src_len;
            }

            if (drop_packet) {
                list_del(&packet->packet_entry);
                packet_free(packet);
            }
        }
    }

    return ret;
}

int socket_recv(struct socket *socket, struct user_buffer buf, size_t len, int flags)
{
    return socket_recvfrom(socket, buf, len, flags, NULL, NULL, 0);
}

int socket_bind(struct socket *socket, const struct sockaddr *addr, socklen_t addrlen)
{
    int ret;

    if (flag_test(&socket->flags, SOCKET_IS_BOUND))
        return -EINVAL;

    ret = socket->proto->ops->bind(socket->proto, socket, addr, addrlen);
    if (ret)
        return ret;

    flag_set(&socket->flags, SOCKET_IS_BOUND);

    return 0;
}

int socket_getsockname(struct socket *socket, struct sockaddr *addr, socklen_t *addrlen)
{
    int ret;

    if (!flag_test(&socket->flags, SOCKET_IS_BOUND))
        return -EINVAL;

    ret = socket->proto->ops->getsockname(socket->proto, socket, addr, addrlen);
    if (ret)
        return ret;

    return 0;
}

int socket_setsockopt(struct socket *socket, int level, int optname, struct user_buffer optval, socklen_t optlen)
{
    return -ENOTSUP;
}

int socket_getsockopt(struct socket *socket, int level, int optname, struct user_buffer optval, struct user_buffer optlen)
{
    return -ENOTSUP;
}

int socket_accept(struct socket *socket, struct sockaddr *addr, socklen_t *addrlen, struct socket **new_socket)
{
    return -ENOTSUP;
}

int socket_connect(struct socket *socket, const struct sockaddr *addr, socklen_t addrlen)
{
    int ret;

    if (!socket->proto->ops->connect)
        return -ENOTSUP;

    ret = socket->proto->ops->connect(socket->proto, socket, addr, addrlen);
    if (ret)
        return ret;

    return 0;
}

int socket_listen(struct socket *socket, int backlog)
{
    return -ENOTSUP;
}

int socket_shutdown(struct socket *socket, int how)
{
    if (socket->proto->ops->shutdown)
        return (socket->proto->ops->shutdown) (socket->proto, socket, how);
    else
        return -ENOTSUP;

    return 0;
}

void socket_release(struct socket *socket)
{
    if (socket->proto->ops->release)
        (socket->proto->ops->release) (socket->proto, socket);
}
