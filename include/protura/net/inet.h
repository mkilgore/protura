#ifndef INCLUDE_PROTURA_NET_INET_H
#define INCLUDE_PROTURA_NET_INET_H

#include <protura/types.h>
#include <protura/socket.h>

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

struct in_addr {
    in_addr_t s_addr;
};

struct sockaddr_in {
    unsigned short    sin_family;    // address family, AF_xxx
    in_port_t         sin_port;
    struct in_addr    sin_addr;
    char              sa_data[8];  // 14 bytes of protocol address
};

#ifdef __KERNEL__

# define PRin_addr "%d.%d.%d.%d"
# define Pin_addr(addr) ((addr) & 0xFF), (((addr) >> 8) & 0xFF), (((addr) >> 16) & 0xFF), (((addr) >> 24) & 0xFF)

#endif

#endif
