/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/snprintf.h>
#include <protura/list.h>
#include <protura/ktimer.h>

#include <protura/net/socket.h>
#include <protura/net/ipv4/tcp.h>
#include <protura/net/ipv4/ipv4.h>
#include <protura/net.h>
#include "ipv4.h"
#include "tcp.h"

static void delack_callback(struct work *work)
{
    struct socket *sock = container_of(work, struct socket, proto_private.tcp.delack.work);

    tcp_send_ack(sock->proto, sock);

    socket_put(sock);
}

void tcp_timers_init(struct socket *sock)
{
    struct work *delack_work = &sock->proto_private.tcp.delack.work;

    work_init_kwork(delack_work, delack_callback);
    flag_set(&delack_work->flags, WORK_ONESHOT);
}

void tcp_delack_timer_start(struct socket *sock, uint32_t ms)
{
    socket_dup(sock);
    kwork_delay_schedule(&sock->proto_private.tcp.delack, ms);
}

void tcp_delack_timer_stop(struct socket *sock)
{
    struct ktimer *timer = &sock->proto_private.tcp.delack.timer;

    timer_del(timer);

    if (!flag_test(&timer->flags, KTIMER_FIRED))
        socket_put(sock);
}

void tcp_timers_reset(struct socket *sock)
{
    tcp_delack_timer_stop(sock);
}
