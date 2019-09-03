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

    n16 total_length;

    n16 id;

    n16 frag_off;
    uint8_t ttl;
    uint8_t protocol;
    n16 csum;
    n32 source_ip;
    n32 dest_ip;
} __packed;

struct tcp_header {
    n16 source;
    n16 dest;
    n32 seq;
    n32 ack_seq;

    uint16_t res1 :4;
    uint16_t hl   :4;
    uint16_t fin  :1;
    uint16_t syn  :1;
    uint16_t rst  :1;
    uint16_t psh  :1;
    uint16_t ack  :1;
    uint16_t urg  :1;
    uint16_t res2 :2;
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

n16 ip_chksum(uint16_t *head, size_t byte_count);
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
