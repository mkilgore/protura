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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <protura/net/arphrd.h>
#include <protura/net/if.h>

#include "arg_parser.h"

#define NETDEV "/proc/net/netdev"

static const char *arg_str = "[Flags] [Interface] [Command]";
static const char *usage_str = "Display and configure network interfaces\n";
static const char *arg_desc_str  = "Interface: The specefic network interface to display.\n"
                                   "Command: Command to perform on the specified interface:\n"
                                   "           addr addr/prefixlen\n"
                                   "           netmask addr\n";

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

static const char *prog;
static int netdev_fd;

static uint32_t netmask_create(int count)
{
    uint32_t mask;

    if (count == 0)
        return 0;

    mask = ~((1 << (32 - count)) - 1);
    return htonl(mask);
}

static void handle_cmd_addr(struct ifreq *ifreq, char *cmd, char *cmd_arg)
{
    struct sockaddr_in *in;
    char *netmask = strchr(cmd_arg, '/');
    int ret;

    if (netmask) {
        int netmask_count;

        *netmask = '\0';
        netmask++;

        netmask_count = atoi(netmask);

        in = (struct sockaddr_in *)&ifreq->ifr_netmask;
        in->sin_family = AF_INET;
        in->sin_addr.s_addr = netmask_create(netmask_count);

        ret = ioctl(netdev_fd, SIOCSIFNETMASK, ifreq);
        if (ret) {
            printf("%s: netmask: %s", ifreq->ifr_name, strerror(errno));
            return ;
        }
    }

    in = (struct sockaddr_in *)&ifreq->ifr_addr;
    in->sin_family = AF_INET;
    in->sin_addr.s_addr = inet_addr(cmd_arg);

    ret = ioctl(netdev_fd, SIOCSIFADDR, ifreq);
    if (ret) {
        printf("%s: addr: %s", ifreq->ifr_name, strerror(errno));
        return ;
    }
}

static void handle_cmd_netmask(struct ifreq *ifreq, char *cmd, char *cmd_arg)
{
    struct sockaddr_in *in;
    int ret;

    in = (struct sockaddr_in *)&ifreq->ifr_netmask;
    in->sin_family = AF_INET;
    in->sin_addr.s_addr = inet_addr(cmd_arg);

    ret = ioctl(netdev_fd, SIOCSIFNETMASK, ifreq);
    if (ret) {
        printf("%s: netmask: %s", ifreq->ifr_name, strerror(errno));
        return ;
    }
}

static void handle_cmd_up(struct ifreq *ifreq, char *cmd, char *cmd_arg)
{
    ioctl(netdev_fd, SIOCGIFFLAGS, ifreq);

    ifreq->ifr_flags |= IFF_UP;

    if (ioctl(netdev_fd, SIOCSIFFLAGS, ifreq) == -1)
        printf("%s: up: %s", ifreq->ifr_name, strerror(errno));
}

static void handle_cmd_down(struct ifreq *ifreq, char *cmd, char *cmd_arg)
{
    ioctl(netdev_fd, SIOCGIFFLAGS, ifreq);

    ifreq->ifr_flags &= ~IFF_UP;

    if (ioctl(netdev_fd, SIOCSIFFLAGS, ifreq) == -1)
        printf("%s: down: %s", ifreq->ifr_name, strerror(errno));
}

static void execute_cmd(struct ifreq *ifreq, char *cmd, char *cmd_arg)
{
    if (strcmp(cmd, "addr") == 0) {
        handle_cmd_addr(ifreq, cmd, cmd_arg);
    } else if (strcmp(cmd, "netmask") == 0) {
        handle_cmd_netmask(ifreq, cmd, cmd_arg);
    } else if (strcmp(cmd, "up") == 0) {
        handle_cmd_up(ifreq, cmd, cmd_arg);
    } else if (strcmp(cmd, "down") == 0) {
        handle_cmd_down(ifreq, cmd, cmd_arg);
    } else {
        printf("%s: Unreconized command \"%s\"\n", prog, cmd);
    }
}

static void display_hwaddr(struct sockaddr *hwaddr)
{
    struct sockaddr_ether *ether;

    switch (hwaddr->sa_family) {
    case ARPHRD_ETHER:
        ether = (struct sockaddr_ether *)hwaddr;
        printf("  Ethernet MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                ether->sa_mac[0], ether->sa_mac[1], ether->sa_mac[2], ether->sa_mac[3], ether->sa_mac[4], ether->sa_mac[5]);
        break;

    case ARPHRD_LOOPBACK:
        printf("  loop\n");
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

static void display_netmask(struct sockaddr *addr)
{
    struct sockaddr_in *inet;
    uint32_t ipv4;

    switch (addr->sa_family) {
    case AF_INET:
        inet = (struct sockaddr_in *)addr;
        ipv4 = ntohl(inet->sin_addr.s_addr);
        printf("  Netmask: %d.%d.%d.%d\n",
                (ipv4 >> 24) & 0xFF, (ipv4 >> 16) & 0xFF, (ipv4 >> 8) & 0xFF, ipv4 & 0xFF);
        break;
    }
}

static void display_flags(int flags)
{
    if (flags & IFF_UP)
        printf("UP ");

    if (flags & IFF_LOOPBACK)
        printf("LOOPBACK ");

    printf("\n");
}

static void display_metrics(struct ifmetrics *metrics)
{
    printf("  Rx Packets: %-8d Rx Bytes: %lld\n", metrics->rx_packets, metrics->rx_bytes);
    printf("  Tx Packets: %-8d Tx Bytes: %lld\n", metrics->tx_packets, metrics->tx_bytes);
}

static void display_iface(struct ifreq *ifreq)
{
    int ret;

    ioctl(netdev_fd, SIOCGIFINDEX, ifreq);
    printf("%s: ", ifreq->ifr_name);

    ret = ioctl(netdev_fd, SIOCGIFFLAGS, ifreq);
    if (!ret)
        display_flags(ifreq->ifr_flags);

    ret = ioctl(netdev_fd, SIOCGIFADDR, ifreq);
    if (!ret)
        display_addr(&ifreq->ifr_addr);

    ret = ioctl(netdev_fd, SIOCGIFNETMASK, ifreq);
    if (!ret)
        display_netmask(&ifreq->ifr_netmask);

    ret = ioctl(netdev_fd, SIOCGIFHWADDR, ifreq);
    if (!ret)
        display_hwaddr(&ifreq->ifr_hwaddr);

    ret = ioctl(netdev_fd, SIOCGIFMETRICS, ifreq);
    if (!ret)
        display_metrics(&ifreq->ifr_metrics);

    printf("\n");
}

int main(int argc, char **argv)
{
    const char *iface = NULL;
    enum arg_index ret;
    struct ifreq ifreq;
    char *cmd = NULL, *cmd_arg = NULL;

    prog = argv[0];

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_EXTRA:
            if (!iface) {
                iface = argarg;
            } else if (!cmd) {
                cmd = argarg;
            } else if (!cmd_arg) {
                cmd_arg = argarg;
            } else {
                printf("%s: Too many arguments\n", argv[0]);
                return 0;
            }

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
        ret = ioctl(netdev_fd, SIOCGIFINDEX, &ifreq);

        if (ifreq.ifr_index == -1 || ret) {
            printf("Unable to access %s: %s\n", iface, strerror(errno));
            return 1;
        }

        if (cmd)
            execute_cmd(&ifreq, cmd, cmd_arg);
        else
            display_iface(&ifreq);
    } else {
        int i = 0;
        int err = 0;
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

