#ifndef INCLUDE_PROTURA_NET_AF_IPV4_H
#define INCLUDE_PROTURA_NET_AF_IPV4_H

#include <uapi/protura/net/ipv4/ipv4.h>
#include <protura/net/ipv4/ip_route.h>

struct packet;
struct address_family;

#define PRin_addr "%d.%d.%d.%d"
#define Pin_addr_inner(addr) ((addr) & 0xFF), (((addr) >> 8) & 0xFF), (((addr) >> 16) & 0xFF), (((addr) >> 24) & 0xFF)
#define Pin_addr(addr) Pin_addr_inner((n32_to_uint32(addr)))

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

static inline in_addr_t sockaddr_in_get_addr(struct sockaddr *sock)
{
    struct sockaddr_in *in = (struct sockaddr_in *)sock;
    return in->sin_addr.s_addr;
}

static inline in_port_t sockaddr_in_get_port(struct sockaddr *sock)
{
    struct sockaddr_in *in = (struct sockaddr_in *)sock;
    return in->sin_port;
}

#endif
