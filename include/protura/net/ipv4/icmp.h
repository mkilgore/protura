#ifndef INCLUDE_PROTURA_NET_PROTO_ICMP_H
#define INCLUDE_PROTURA_NET_PROTO_ICMP_H

#include <protura/types.h>

void icmp_init(void);
void icmp_init_delay(void);

struct icmp_socket_private {
    n16 src_port;
    n16 dest_port;
};


#endif
