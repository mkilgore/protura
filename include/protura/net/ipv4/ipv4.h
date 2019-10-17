#ifndef INCLUDE_PROTURA_NET_AF_IPV4_H
#define INCLUDE_PROTURA_NET_AF_IPV4_H

#include <protura/net/sockaddr.h>
#include <protura/net/ipv4/ip_route.h>

typedef __kn32 in_addr_t;
typedef __kn16 in_port_t;

struct in_addr {
    in_addr_t s_addr;
};

struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t   sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

#define IPPROTO_IP   0
#define IPPROTO_ICMP 1
#define IPPROTO_TCP  6
#define IPPROTO_UDP  17
#define IPOROTO_RAW  255
#define IPOROTO_MAX  256

#define INADDR_ANY       ((in_addr_t) __kn32_make(0x00000000))
#define INADDR_BROADCAST ((in_addr_t) __kn32_make(0xFFFFFFFF))

#ifdef __KERNEL__

struct packet;
struct address_family;

# define PRin_addr "%d.%d.%d.%d"
# define Pin_addr_inner(addr) ((addr) & 0xFF), (((addr) >> 8) & 0xFF), (((addr) >> 16) & 0xFF), (((addr) >> 24) & 0xFF)
# define Pin_addr(addr) Pin_addr_inner((n32_to_uint32(addr)))

#define in_addr_mask(addr, mask) n32_make(n32_to_uint32(addr) & n32_to_uint32(mask))

void ip_init(void);

void ip_rx(struct address_family *afamily, struct packet *packet);

in_addr_t inet_addr(const char *ip);

struct ipv4_socket_private {
    int proto;

    in_addr_t src_addr; // bind_addr
    in_addr_t dest_addr;

    in_port_t src_port;
    in_port_t dest_port;

    struct ip_route_entry route;
};

static inline void sockaddr_in_assign(struct sockaddr *sock, in_addr_t addr, in_port_t port)
{
    struct sockaddr_in *in = (struct sockaddr_in *)sock;
    in->sin_family = AF_INET;
    in->sin_addr.s_addr = addr;
    in->sin_port = port;
}

static inline void sockaddr_in_assign_addr(struct sockaddr *sock, in_addr_t addr)
{
    struct sockaddr_in *in = (struct sockaddr_in *)sock;
    in->sin_addr.s_addr = addr;
}

static inline void sockaddr_in_assign_port(struct sockaddr *sock, in_port_t port)
{
    struct sockaddr_in *in = (struct sockaddr_in *)sock;
    in->sin_port = port;
}

#endif

#endif
