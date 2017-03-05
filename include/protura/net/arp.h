#ifndef INCLUDE_PROTURA_NET_ARP_H
#define INCLUDE_PROTURA_NET_ARP_H

#include <protura/net/af/ipv4.h>

struct packet;
struct net_interface;

int arp_ipv4_to_mac(in_addr_t inet_addr, uint8_t *mac, struct net_interface *iface);
void arp_init(void);

#endif
