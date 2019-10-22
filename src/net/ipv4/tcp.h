#ifndef SRC_NET_AF_IPV4_TCP_H
#define SRC_NET_AF_IPV4_TCP_H

#include <protura/net/types.h>
#include <protura/net/proto.h>

struct pseudo_header {
    n32 saddr;
    n32 daddr;
    uint8_t zero;
    uint8_t proto;
    n16 len;
} __packed;

void tcp_rx(struct protocol *proto, struct socket *sock, struct packet *packet);

void tcp_send_syn(struct protocol *proto, struct socket *sock);
void tcp_send_ack(struct protocol *proto, struct socket *sock);
void tcp_send(struct protocol *proto, struct socket *sock, struct packet *packet);

void tcp_recv_data(struct protocol *proto, struct socket *sock, struct packet *packet);

void tcp_timers_init(struct socket *sock);
void tcp_timers_reset(struct socket *sock);
void tcp_delack_timer_start(struct socket *sock, uint32_t ms);
void tcp_delack_timer_stop(struct socket *sock);

n16 tcp_checksum(struct pseudo_header *header, const char *data, size_t len);
n16 tcp_checksum_packet(struct packet *packet);

static inline int tcp_seq_before(uint32_t seq1, uint32_t seq2)
{
    return ((int32_t)(seq1 - seq2)) < 0;
}

static inline int tcp_seq_after(uint32_t seq1, uint32_t seq2)
{
    return ((int32_t)(seq1 - seq2)) > 0;
}

/* Checks that seq1 < seq2 < seq3 */
static inline int tcp_seq_between(uint32_t seq1, uint32_t seq2, uint32_t seq3)
{
    return tcp_seq_before(seq1, seq2) && tcp_seq_before(seq2, seq3);
}

#endif
