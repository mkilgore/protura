#ifndef INCLUDE_PROTURA_NET_SOCKADDR_H
#define INCLUDE_PROTURA_NET_SOCKADDR_H

#include <protura/types.h>

struct sockaddr {
    unsigned short    sa_family;    // address family, AF_xxx
    char              sa_data[14];  // 14 bytes of protocol address
};

typedef __ksize_t socklen_t;
typedef __kn16    sa_family_t;

#define AF_INET 2
#define AF_MAX 3

#define PF_INET AF_INET
#define PF_MAX  AF_MAX

#define NET_MAXID AF_MAX

#define SOL_SOCKET 1

#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#define SOCK_RAW    3

#define SOCK_NONBLOCK 0x4000
#define SOCK_CLOEXEC  0x40000

#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2

#ifdef __KERNEL__

#define AF_ARP (AF_MAX + 1)

#endif

#endif
