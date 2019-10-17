#ifndef INCLUDE_PROTURA_NET_PROTO_TCP_H
#define INCLUDE_PROTURA_NET_PROTO_TCP_H

#include <protura/types.h>
#include <protura/net/types.h>

#ifdef __KERNEL__
# include <protura/list.h>

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

struct tcp_socket_private {
    int tcp_state;

    uint32_t iss;
    uint32_t irs;

    uint32_t rcv_nxt;
    uint32_t rcv_up;
    uint32_t rcv_wnd;

    uint32_t snd_nxt;
    uint32_t snd_una;
    uint32_t snd_up;
    uint32_t snd_wnd;
    uint32_t snd_wl1;
    uint32_t snd_wl2;
};

union tcp_flags {
    uint8_t flags;
    struct {
        uint8_t fin :1;
        uint8_t syn :1;
        uint8_t rst :1;
        uint8_t psh :1;
        uint8_t ack :1;
        uint8_t urg :1;
        uint8_t res :2;
    };
};


struct tcp_packet_cb {
    uint32_t seq;
    uint32_t ack_seq;
    uint32_t window;
    union tcp_flags flags;
};

#endif

#endif
