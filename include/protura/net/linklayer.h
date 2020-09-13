#ifndef INCLUDE_PROTURA_NET_LINKLAYER_H
#define INCLUDE_PROTURA_NET_LINKLAYER_H

struct packet;

void packet_linklayer_rx(struct packet *packet);
int packet_linklayer_tx(struct packet *packet);

#endif
