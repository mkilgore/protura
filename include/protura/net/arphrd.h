#ifndef INCLUDE_PROTURA_NET_ARPHRD_H
#define INCLUDE_PROTURA_NET_ARPHRD_H

#define ETH_P_ARP 0x0806
#define ETH_P_IP  0x0800

#define ARPHRD_ETHER 1

#define	ARPOP_REQUEST 1
#define	ARPOP_REPLY   2

struct sockaddr_ether {
    unsigned short sa_family;
    char sa_mac[6]; /* Mac address */
    char sa_data[8];  /* Padding */
};

#endif
