#ifndef SRC_NET_AF_IPV4_IPV4_H
#define SRC_NET_AF_IPV4_IPV4_H

#include <protura/compiler.h>
#include <protura/fs/procfs.h>
#include <protura/net/packet.h>
#include <protura/net/sockaddr.h>
#include <protura/net/af.h>

extern struct procfs_entry_ops ipv4_route_ops;

extern struct procfs_dir *ipv4_dir_procfs;

struct ip_header {
    uint8_t ihl :4;
    uint8_t version :4;

    uint8_t tos;

    uint16_t total_length;

    uint16_t id;

    uint16_t frag_off;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t csum;
    uint32_t source_ip;
    uint32_t dest_ip;
} __packed;

struct tcp_header {
    n16 source;
    n16 dest;
    n32 seq;
    n32 ack_seq;

    n16 res1 :4;
    n16 hl   :4;
    n16 fin  :1;
    n16 syn  :1;
    n16 rst  :1;
    n16 psh  :1;
    n16 ack  :1;
    n16 urg  :1;
    n16 res2 :2;
    n16 window;
    n16 check;
    n16 urg_ptr;
} __packed;

struct ip_lookup {
    int proto;

    in_addr_t src_addr;
    in_port_t src_port;

    in_addr_t dest_addr;
    in_port_t dest_port;
};

struct address_family_ip {
    struct address_family af;

    /* FIXME: Replace this with a hash-table of the proto at least */
    mutex_t lock;
    list_head_t raw_sockets;
    list_head_t sockets;
};

extern struct address_family_ip ip_address_family;

void __ipaf_add_socket(struct address_family_ip *af, struct socket *sock);
void __ipaf_remove_socket(struct address_family_ip *af, struct socket *sock);

__must_check struct socket *__ipaf_find_socket(struct address_family_ip *af, struct ip_lookup *addr, int total_max_score);

uint16_t ip_chksum(uint16_t *data, size_t byte_count);
int ip_process_sockaddr(struct packet *packet, const struct sockaddr *addr, socklen_t len);
int ip_tx(struct packet *packet);

void tcp_lookup_fill(struct ip_lookup *, struct packet *);
void udp_lookup_fill(struct ip_lookup *, struct packet *);

void ip_raw_init(void);

#ifdef CONFIG_KERNEL_LOG_IP
# define kp_ip(str, ...) kp(KP_NORMAL, "IP: " str, ## __VA_ARGS__)
#else
# define kp_ip(str, ...) do { ; } while (0)
#endif

#ifdef CONFIG_KERNEL_LOG_ICMP
# define kp_icmp(str, ...) kp(KP_NORMAL, "ICMP: " str, ## __VA_ARGS__)
#else
# define kp_icmp(str, ...) do { ; } while (0)
#endif

#ifdef CONFIG_KERNEL_LOG_UDP
# define kp_udp(str, ...) kp(KP_NORMAL, "UDP: " str, ## __VA_ARGS__)
#else
# define kp_udp(str, ...) do { ; } while (0)
#endif

#ifdef CONFIG_KERNEL_LOG_TCP
# define kp_tcp(str, ...) kp(KP_NORMAL, "TCP: " str, ## __VA_ARGS__)
#else
# define kp_tcp(str, ...) do { ; } while (0)
#endif

#endif
