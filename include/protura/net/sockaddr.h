#ifndef INCLUDE_PROTURA_NET_SOCKADDR_H
#define INCLUDE_PROTURA_NET_SOCKADDR_H

#include <protura/net/arphrd.h>

struct sockaddr {
    unsigned short    sa_family;    // address family, AF_xxx
    char              sa_data[14];  // 14 bytes of protocol address
};

#endif
