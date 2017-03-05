// route - Display and modify the IP routing table
#define UTILITY_NAME "route"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <protura/net/route.h>

#include "arg_parser.h"

static const char *arg_str = "[Flags] [Command] [options]";
static const char *usage_str = "Display and configure the OS routing table.\n";
static const char *arg_desc_str  = "Command: Optional command to perform on the routing table.\n";

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

#define ROUTE_BASE "/proc/net"

const char *prog;
const char *route_base = ROUTE_BASE;
const char *address_family = "ipv4";
int route_fd = -1;

int open_route_fd(void)
{
    char path[256];

    snprintf(path, sizeof(path), "%s/%s/route", route_base, address_family);

    route_fd = open(path, O_RDONLY);
    if (route_fd == -1) {
        perror(path);
        return 1;
    }

    return 0;
}

void close_route_fd(void)
{
     if (route_fd != -1)
         close(route_fd);
}

void display_routing_table(void)
{
    struct route_entry entries[40];
    int entry_count, i;
    struct sockaddr_in *in;

    entry_count = read(route_fd, entries, sizeof(entries)) / sizeof(struct route_entry);

    /*      0               16              32              48    51 */
    printf("Destination     Gateway         Genmask         Flags Iface\n");
    for (i = 0; i < entry_count; i++) {
        in = (struct sockaddr_in *)&entries[i].dest;
        if (in->sin_addr.s_addr == INADDR_ANY)
            printf("default         ");
        else
            printf("%-16s", inet_ntoa(in->sin_addr));

        in = (struct sockaddr_in *)&entries[i].gateway;
        printf("%-16s", inet_ntoa(in->sin_addr));

        in = (struct sockaddr_in *)&entries[i].netmask;
        printf("%-16s", inet_ntoa(in->sin_addr));

        printf("%c%c    ",
                (entries[i].flags & RT_ENT_UP)? 'U': ' ',
                (entries[i].flags & RT_ENT_GATEWAY)? 'G': ' ');
        printf("%s\n", entries[i].netdev);
    }
}

int parse_route(char **args, struct route_entry *rt)
{
    enum {
        EXPECTING_NONE,
        EXPECTING_TARGET,
        EXPECTING_NETMASK,
        EXPECTING_GATEWAY,
        EXPECTING_DEV,
    } state = EXPECTING_TARGET;
    int have_state[5] = { 0 };
    struct sockaddr_in *in;

    memset(rt, 0, sizeof(*rt));

    for (; *args; args++) {
        if (have_state[state]) {
            printf("%s: Unexpected argument \"%s\"\n", prog, *args);
            return 1;
        }

        switch (state) {
        case EXPECTING_NONE:
            if (strcmp(*args, "netmask") == 0) {
                state = EXPECTING_NETMASK;
            } else if (strcmp(*args, "gw") == 0) {
                state = EXPECTING_GATEWAY;
            } else if (strcmp(*args, "dev") == 0) {
                state = EXPECTING_DEV;
            } else {
                strncpy(rt->netdev, *args, sizeof(rt->netdev));
                have_state[state] = 1;
            }
            break;

        case EXPECTING_TARGET:
            have_state[EXPECTING_TARGET] = 1;
            in = (struct sockaddr_in *)&rt->dest;
            in->sin_family = AF_INET;

            if (strcmp(*args, "default") == 0)
                in->sin_addr.s_addr = INADDR_ANY;
            else
                in->sin_addr.s_addr = inet_addr(*args);

            state = EXPECTING_NONE;
            break;

        case EXPECTING_GATEWAY:
            have_state[EXPECTING_GATEWAY] = 1;
            in = (struct sockaddr_in *)&rt->gateway;
            in->sin_family = AF_INET;
            in->sin_addr.s_addr = inet_addr(*args);

            rt->flags |= RT_ENT_GATEWAY;

            state = EXPECTING_NONE;
            break;

        case EXPECTING_NETMASK:
            have_state[EXPECTING_NETMASK] = 1;
            in = (struct sockaddr_in *)&rt->netmask;
            in->sin_family = AF_INET;
            in->sin_addr.s_addr = inet_addr(*args);

            state = EXPECTING_NONE;
            break;

        case EXPECTING_DEV:
            have_state[EXPECTING_DEV] = 1;
            strncpy(rt->netdev, *args, sizeof(rt->netdev));

            state = EXPECTING_NONE;
            break;
        }
    }

    if (!have_state[EXPECTING_TARGET])
        return 1;

    return 0;
}

void add_entry(char **args)
{
    struct route_entry rt;

    if (parse_route(args, &rt))
        return ;

    if (ioctl(route_fd, SIOCADDRT, &rt) == -1)
        printf("%s: add: %s\n", prog, strerror(errno));
}

void del_entry(char **args)
{
    struct route_entry rt;

    if (parse_route(args, &rt))
        return ;

    if (ioctl(route_fd, SIOCDELRT, &rt) == -1)
        printf("%s: add: %s\n", prog, strerror(errno));
}

int main(int argc, char **argv)
{
    enum {
        ACTION_DISPLAY,
        ACTION_ADD,
        ACTION_DEL,
    } action = ACTION_DISPLAY;
    enum arg_index ret;

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
            if (strcmp(argarg, "add") == 0) {
                action = ACTION_ADD;
                goto parsing_done;
            } else if (strcmp(argarg, "del") == 0) {
                action = ACTION_DEL;
                goto parsing_done;
            }
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }
  parsing_done:

    if (open_route_fd())
        return 0;

    switch (action) {
    case ACTION_ADD:
        add_entry(argv + current_arg);
        break;

    case ACTION_DEL:
        del_entry(argv + current_arg);
        break;

    case ACTION_DISPLAY:
        display_routing_table();
        break;
    }

    close_route_fd();

    return 0;
}

