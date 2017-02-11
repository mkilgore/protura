#ifndef INCLUDE_PROTURA_SOCKET_H
#define INCLUDE_PROTURA_SOCKET_H

#include <protura/types.h>

static inline uint32_t htonl(uint32_t hostl)
{
    return ((hostl & 0xFF000000) >> 24)
         | ((hostl & 0x00FF0000) >> 8)
         | ((hostl & 0x0000FF00) << 8)
         | ((hostl & 0x000000FF) << 24);
}

static inline uint16_t htons(uint16_t hosts)
{
    return ((hosts & 0xFF00) >> 8)
         | ((hosts & 0x00FF) << 8);
}

#define ntohl(netl) htonl(netl)
#define ntohs(nets) htons(nets)

#define AF_INET 2

#endif
