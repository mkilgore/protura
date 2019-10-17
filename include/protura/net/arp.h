#ifndef INCLUDE_PROTURA_NET_ARP_H
#define INCLUDE_PROTURA_NET_ARP_H

#include <protura/net/ipv4/ipv4.h>

struct packet;
struct net_interface;

void arp_tx(struct packet *);

void arp_init(void);

#endif
