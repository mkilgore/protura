#ifndef SRC_NET_AF_IPV4_IPV4_H
#define SRC_NET_AF_IPV4_IPV4_H

#include <protura/compiler.h>
#include <protura/kparam.h>
#include <protura/fs/procfs.h>
#include <protura/net/packet.h>
#include <protura/net/sockaddr.h>
#include <protura/net/proto.h>
#include <protura/net/af.h>

extern struct procfs_entry_ops ipv4_route_ops;

extern struct procfs_dir *ipv4_dir_procfs;

/* A very conservative max-segment-size for all IP-based packets/protocols */
#define IPV4_PACKET_MSS 1024

struct ip_header {
    uint8_t ihl :4;
    uint8_t version :4;

    uint8_t tos;

    n16 total_length;

    n16 id;

    n16 frag_off;
    uint8_t ttl;
    uint8_t protocol;
    n16 csum;
    n32 source_ip;
    n32 dest_ip;
} __packed;

enum {
    TCP_FIN = 1 << 0,
    TCP_SYN = 1 << 1,
    TCP_RST = 1 << 2,
    TCP_PSH = 1 << 3,
    TCP_ACK = 1 << 4,
    TCP_URG = 1 << 5,
};

struct tcp_header {
    n16 source;
    n16 dest;
    n32 seq;
    n32 ack_seq;

    uint16_t res1 :4;
    uint16_t hl   :4;

    union tcp_flags flags;

    n16 window;
    n16 check;
    n16 urg_ptr;
} __packed;

struct ip_lookup {
    uint8_t proto;

    in_addr_t src_addr;
    in_port_t src_port;

    in_addr_t dest_addr;
    in_port_t dest_port;
};

struct address_family_ip {
    struct address_family af;

    mutex_t lock;
    list_head_t raw_sockets;
    list_head_t sockets;
};

extern struct address_family_ip ip_address_family;

void __ipaf_add_socket(struct address_family_ip *af, struct socket *sock);
void __ipaf_remove_socket(struct address_family_ip *af, struct socket *sock);

__must_check struct socket *__ipaf_find_socket(struct address_family_ip *af, struct ip_lookup *addr, int total_max_score);

n16 ip_chksum(uint16_t *head, size_t byte_count);
int ip_packet_fill_route_addr(struct socket *sock, struct packet *packet, const struct sockaddr *addr, socklen_t len);
int ip_packet_fill_route(struct socket *sock, struct packet *packet);
int ip_packet_fill_raw(struct packet *packet, in_addr_t dest_addr);
void ip_tx(struct packet *packet);
void ip_release(struct address_family *family, struct socket *sock);

void tcp_lookup_fill(struct ip_lookup *, struct packet *);
void udp_lookup_fill(struct ip_lookup *, struct packet *);

struct protocol *udp_get_proto(void);
struct protocol *tcp_get_proto(void);
struct protocol *ip_raw_get_proto(void);

extern struct file_ops udp_proc_file_ops;
extern struct file_ops ip_raw_proc_file_ops;
extern struct file_ops tcp_proc_file_ops;

extern int ip_max_log_level;
extern int icmp_max_log_level;
extern int udp_max_log_level;
extern int tcp_max_log_level;

#define kp_ip_check_level(lvl, str, ...) \
    kp_check_level((lvl), ip_max_log_level, "ip: " str, ## __VA_ARGS__)

#define kp_icmp_check_level(lvl, str, ...) \
    kp_check_level((lvl), icmp_max_log_level, "icmp: " str, ## __VA_ARGS__)

#define kp_udp_check_level(lvl, str, ...) \
    kp_check_level((lvl), udp_max_log_level, "udp: " str, ## __VA_ARGS__)

#define kp_tcp_check_level(lvl, str, ...) \
    kp_check_level((lvl), tcp_max_log_level, "tcp: " str, ## __VA_ARGS__)

#define kp_ip_trace(str, ...)   kp_ip_check_level(KP_TRACE, str, ## __VA_ARGS__)
#define kp_ip_debug(str, ...)   kp_ip_check_level(KP_DEBUG, str, ## __VA_ARGS__)
#define kp_ip(str, ...)         kp_ip_check_level(KP_NORMAL, str, ## __VA_ARGS__)
#define kp_ip_warning(str, ...) kp_ip_check_level(KP_WARNING, str, ## __VA_ARGS__)
#define kp_ip_error(str, ...)   kp_ip_check_level(KP_ERROR, str, ## __VA_ARGS__)

#define kp_icmp_trace(str, ...)   kp_icmp_check_level(KP_TRACE, str, ## __VA_ARGS__)
#define kp_icmp_debug(str, ...)   kp_icmp_check_level(KP_DEBUG, str, ## __VA_ARGS__)
#define kp_icmp(str, ...)         kp_icmp_check_level(KP_NORMAL, str, ## __VA_ARGS__)
#define kp_icmp_warning(str, ...) kp_icmp_check_level(KP_WARNING, str, ## __VA_ARGS__)
#define kp_icmp_error(str, ...)   kp_icmp_check_level(KP_ERROR, str, ## __VA_ARGS__)

#define kp_udp_trace(str, ...)   kp_udp_check_level(KP_TRACE, str, ## __VA_ARGS__)
#define kp_udp_debug(str, ...)   kp_udp_check_level(KP_DEBUG, str, ## __VA_ARGS__)
#define kp_udp(str, ...)         kp_udp_check_level(KP_NORMAL, str, ## __VA_ARGS__)
#define kp_udp_warning(str, ...) kp_udp_check_level(KP_WARNING, str, ## __VA_ARGS__)
#define kp_udp_error(str, ...)   kp_udp_check_level(KP_ERROR, str, ## __VA_ARGS__)

#define kp_tcp_trace(str, ...)   kp_tcp_check_level(KP_TRACE, str, ## __VA_ARGS__)
#define kp_tcp_debug(str, ...)   kp_tcp_check_level(KP_DEBUG, str, ## __VA_ARGS__)
#define kp_tcp(str, ...)         kp_tcp_check_level(KP_NORMAL, str, ## __VA_ARGS__)
#define kp_tcp_warning(str, ...) kp_tcp_check_level(KP_WARNING, str, ## __VA_ARGS__)
#define kp_tcp_error(str, ...)   kp_tcp_check_level(KP_ERROR, str, ## __VA_ARGS__)

#endif
