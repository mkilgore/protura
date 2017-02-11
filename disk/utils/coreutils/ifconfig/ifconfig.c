// ifconfig - Configure a Network Interface
#define UTILITY_NAME "ifconfig"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <protura/net/netdevice.h>
#include <protura/net/inet.h>
#include <protura/socket.h>

#include "arg_parser.h"

#define NETDEV "/proc/netdev"

static const char *arg_str = "[Interface]";
static const char *usage_str = "Display and configure network interfaces\n";
static const char *arg_desc_str  = "Interface: The specefic network interface to display.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(last, NULL, '\0', 0, NULL, NULL)

enum arg_index {
  ARG_EXTRA = ARG_PARSER_EXTRA,
  ARG_ERR = ARG_PARSER_ERR,
  ARG_DONE = ARG_PARSER_DONE,
#define X(enu, ...) ARG_ENUM(enu)
  XARGS
#undef X
};

static const struct arg args[] = {
#define X(...) CREATE_ARG(__VA_ARGS__)
  XARGS
#undef X
};

static int netdev_fd;

static void display_hwaddr(struct sockaddr *hwaddr)
{
    struct sockaddr_ether *ether;

    switch (hwaddr->sa_family) {
    case ARPHRD_ETHER:
        ether = (struct sockaddr_ether *)hwaddr;
        printf("  Ethernet MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                ether->sa_mac[0], ether->sa_mac[1], ether->sa_mac[2], ether->sa_mac[3], ether->sa_mac[4], ether->sa_mac[5]);
        break;
    }
}

static void display_addr(struct sockaddr *addr)
{
    struct sockaddr_in *inet;
    uint32_t ipv4;

    switch (addr->sa_family) {
    case AF_INET:
        inet = (struct sockaddr_in *)addr;
        ipv4 = ntohl(inet->sin_addr.s_addr);
        printf("  IPv4: %d.%d.%d.%d\n",
                (ipv4 >> 24) & 0xFF, (ipv4 >> 16) & 0xFF, (ipv4 >> 8) & 0xFF, ipv4 & 0xFF);
        break;
    }
}

static void display_iface(struct ifreq *ifreq)
{
    int ret;

    ioctl(netdev_fd, SIOCGIFINDEX, &ifreq);
    printf("%s: %d\n", ifreq->ifr_name, ifreq->ifr_index);

    ret = ioctl(netdev_fd, SIOCGIFADDR, ifreq);
    if (!ret)
        display_addr(&ifreq->ifr_addr);

    ret = ioctl(netdev_fd, SIOCGIFHWADDR, ifreq);
    if (!ret)
        display_hwaddr(&ifreq->ifr_hwaddr);

    printf("\n");
}

int main(int argc, char **argv)
{
    const char *iface = NULL;
    enum arg_index ret;
    struct ifreq ifreq;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_EXTRA:
            iface = argarg;
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    netdev_fd = open(NETDEV, O_RDONLY);
    if (netdev_fd == -1) {
        perror(NETDEV);
        return 1;
    }

    if (iface) {
        strncpy(ifreq.ifr_name, iface, IFNAMSIZ);
        ioctl(netdev_fd, SIOCGIFINDEX, &ifreq);

        if (ifreq.ifr_index == -1) {
            printf("Unable to access %s: %s\n", iface, strerror(errno));
            return 1;
        }

        display_iface(&ifreq);
    } else {
        int i = 0;
        int err;
        int found = 0;

        for (; !err; i++) {
            ifreq.ifr_index = i;
            err = ioctl(netdev_fd, SIOCGIFNAME, &ifreq);
            if (!err) {
                found = 1;
                display_iface(&ifreq);
            }
        }

        if (!found)
            printf("No Network Interfaces found\n");
    }

    close(netdev_fd);

    return 0;
}

