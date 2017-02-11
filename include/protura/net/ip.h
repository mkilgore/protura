#ifndef INCLUDE_PROTURA_NET_IP_H
#define INCLUDE_PROTURA_NET_IP_H

#include <protura/net.h>
#include <protura/net/inet.h>

/*
 * Submit packets to the IPv4 handler.
 */
void ip_rx(struct packet *packet);
void ip_tx(struct packet *packet);

#endif
