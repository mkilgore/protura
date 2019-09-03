#ifndef INCLUDE_PROTURA_NET_PROTO_TCP_H
#define INCLUDE_PROTURA_NET_PROTO_TCP_H

#include <protura/types.h>

enum {
    TCP_ESTABLISHED = 1,
    TCP_SYN_SENT,
    TCP_SYN_RECV,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_TIME_WAIT,
    TCP_CLOSE,
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK,
    TCP_LISTEN,
    TCP_CLOSING
};

#ifdef __KERNEL__

#include <protura/list.h>

struct protocol;
struct packet;

void tcp_init(void);

struct tcp_socket_private {
    int tcp_state;

    n32 rcv_next;
    n32 rcv_up;
    n32 rcv_window;

    n32 snd_next;
    n32 snd_una;
    n32 snd_up;
    n32 snd_wl1;
    n32 snd_wl2;

    list_head_t packet_queue;
};

#endif

#endif
