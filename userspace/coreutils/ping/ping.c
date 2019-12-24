// ping - Seng ICMP packets
#define UTILITY_NAME "ping"

#include "common.h"

#define __BSD_VISIBLE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <protura/list.h>

#include "arg_parser.h"

struct icmphdr
{
  u_int8_t type;		/* message type */
  u_int8_t code;		/* type sub-code */
  u_int16_t checksum;
  union
  {
    struct
    {
      u_int16_t	id;
      u_int16_t	sequence;
    } echo;			/* echo datagram */
    u_int32_t	gateway;	/* gateway address */
    struct
    {
      u_int16_t	__glibc_reserved;
      u_int16_t	mtu;
    } frag;			/* path mtu discovery */
  } un;
};

#define ICMP_ECHOREPLY		0	/* Echo Reply			*/
#define ICMP_DEST_UNREACH	3	/* Destination Unreachable	*/
#define ICMP_SOURCE_QUENCH	4	/* Source Quench		*/
#define ICMP_REDIRECT		5	/* Redirect (change route)	*/
#define ICMP_ECHO		8	/* Echo Request			*/
#define ICMP_TIME_EXCEEDED	11	/* Time Exceeded		*/
#define ICMP_PARAMETERPROB	12	/* Parameter Problem		*/
#define ICMP_TIMESTAMP		13	/* Timestamp Request		*/
#define ICMP_TIMESTAMPREPLY	14	/* Timestamp Reply		*/
#define ICMP_INFO_REQUEST	15	/* Information Request		*/
#define ICMP_INFO_REPLY		16	/* Information Reply		*/
#define ICMP_ADDRESS		17	/* Address Mask Request		*/
#define ICMP_ADDRESSREPLY	18	/* Address Mask Reply		*/
#define NR_ICMP_TYPES		18

static const char *arg_str = "[Flags] Destination";
static const char *usage_str = "Send ICMP Packets to chosen destination.\n";
static const char *arg_desc_str  = "Destination: IP to send packets too.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(count, "count", 'c', 1, "count", "Number of pings to send") \
    X(flood, "flood", 'f', 0, NULL, "Rapid echo and reply display") \
    X(interval, "interval", 'i', 1, "ms", "Wait interval milli-seconds before sending ping") \
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

#define PACKETSIZE	64
struct packet
{
	struct icmphdr hdr;
	char msg[PACKETSIZE-sizeof(struct icmphdr)];
};

unsigned short checksum(void *b, int len)
{
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;

    if (len == 1)
        sum += *(unsigned char *)buf;

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;

    return result;
}

struct icmp_msg {
    list_node_t icmp_entry;
    struct timeval sent;
    int seq;
};

struct icmp_stats {
    int sent_packets;
    int recv_packets;
    int expired_packets;
    struct timeval average_time;
};

static struct icmp_stats stats;

static list_head_t sent_list = LIST_HEAD_INIT(sent_list);
static list_head_t expired_list = LIST_HEAD_INIT(expired_list);

static int msg_cnt = 1;
static int msg_id = 0;
static int interval_ms = 1000;
static int expiration_ms = 1000;
static int exit_ping = 0;
static int total_msgs_to_send = -1;
static int flood_display = 0;

static struct sockaddr dest_addr;
static int ping_fd;

static void handle_int(int sig)
{
    exit_ping = 1;
}

static int timeval_to_ms(struct timeval tm)
{
    return (tm.tv_sec * 1000 + tm.tv_usec);
}

static int timeval_elapsed_ms(struct timeval t1, struct timeval t2)
{
    struct timeval tmp;
    int elapsed;
    timersub(&t2, &t1, &tmp);
    elapsed = timeval_to_ms(tmp);
    return elapsed;
}

static int ping_longest_elapsed(struct timeval current)
{
    struct icmp_msg *msg = list_last_entry(&sent_list, struct icmp_msg, icmp_entry);
    if (list_empty(&sent_list))
        return -1;

    int elapsed = timeval_elapsed_ms(msg->sent, current);

    return elapsed;
}

void ping_expire_msgs(struct timeval current)
{
    struct icmp_msg *msg, *next;

    for (msg = list_first_entry(&sent_list, struct icmp_msg, icmp_entry);
         !list_ptr_is_head(&sent_list, &msg->icmp_entry);
         msg = next) {
        next = list_next_entry(msg, icmp_entry);

        int elapsed = timeval_elapsed_ms(msg->sent, current);
        if (expiration_ms > elapsed)
            continue;

        if (!flood_display)
            printf("seq=%d Expired\n", msg->seq);

        list_del(&msg->icmp_entry);
        list_add(&expired_list, &msg->icmp_entry);
        stats.expired_packets++;
    }
}

void ping_send_msg(struct timeval current)
{
    struct packet packet;
    struct icmp_msg *msg;
    int i;

    memset(&packet, 0, sizeof(packet));
    packet.hdr.type = ICMP_ECHO;
    packet.hdr.un.echo.id = msg_id;

    for (i = 0; i < sizeof(packet.msg)-1; i++ )
        packet.msg[i] = i+'0';

    packet.msg[i] = 0;
    packet.hdr.un.echo.sequence = msg_cnt++;
    packet.hdr.checksum = checksum(&packet, sizeof(packet));

    if (sendto(ping_fd, &packet, sizeof(packet), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0 ) {
        perror("sendto");
        return ;
    }

    if (flood_display)
        putchar('.');

    msg = malloc(sizeof(*msg));
    memset(msg, 0, sizeof(*msg));
    list_node_init(&msg->icmp_entry);

    msg->sent = current;
    msg->seq = packet.hdr.un.echo.sequence;

    list_add_tail(&sent_list, &msg->icmp_entry);

    stats.sent_packets++;
}

int ping_recv_msg(void)
{
    struct packet pkt;
    struct sockaddr_in recv_addr;
    socklen_t len = sizeof(recv_addr);
    struct icmp_msg *msg;
    struct timeval recv_time;
    int bytes;

    bytes = recvfrom(ping_fd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&recv_addr, &len);

    if (bytes == -1)
        return 1;

    if (pkt.hdr.type != ICMP_ECHOREPLY)
        return 0;

    if (pkt.hdr.un.echo.id != msg_id)
        return 0;

    gettimeofday(&recv_time, NULL);

    list_foreach_entry(&sent_list, msg, icmp_entry) {
        if (msg->seq == pkt.hdr.un.echo.sequence) {
            int elapsed = timeval_elapsed_ms(msg->sent, recv_time);

            if (!flood_display)
                printf("%d Bytes from %s: seq=%d time=%d\n",
                        bytes, inet_ntoa(recv_addr.sin_addr), pkt.hdr.un.echo.sequence, elapsed);
            else
                putchar('\b');

            list_del(&msg->icmp_entry);
            free(msg);

            stats.recv_packets++;
            return 0;
        }
    }

    list_foreach_entry(&expired_list, msg, icmp_entry) {
        if (msg->seq == pkt.hdr.un.echo.sequence) {
            int elapsed = timeval_elapsed_ms(msg->sent, recv_time);

            if (!flood_display)
                printf("%d Bytes from %s: seq=%d time=%d (Expired)\n",
                        bytes, inet_ntoa(recv_addr.sin_addr), pkt.hdr.un.echo.sequence, elapsed);
            else
                putchar('\b');

            list_del(&msg->icmp_entry);
            free(msg);
            return 0;
        }
    }

    return 0;
}

void ping_loop(void)
{
    int send_ping = 0;
    struct pollfd fds[1] = { 0 };
    struct timeval last_msg = { 0 }, current;
    struct sigaction act, old;

    memset(&act, 0, sizeof(act));
    act.sa_handler = handle_int;

    sigaction(SIGINT, &act, &old);

    fds[0].fd = ping_fd;
    fds[0].events = POLLIN;

    gettimeofday(&last_msg, NULL);
    send_ping = 1;

    while (!exit_ping) {
        int elapsed;
        int next_expiration;
        int sleep_time;

        gettimeofday(&current, NULL);

        elapsed = timeval_elapsed_ms(last_msg, current);

        if (elapsed >= interval_ms)
            send_ping = 1;

        if (send_ping) {
            send_ping = 0;

            ping_send_msg(current);

            last_msg = current;
            elapsed = 0;
        }

        ping_expire_msgs(current);

        next_expiration = ping_longest_elapsed(current);

        if (next_expiration > -1 && expiration_ms - next_expiration > interval_ms - elapsed)
            sleep_time = expiration_ms - next_expiration;
        else
            sleep_time = interval_ms - elapsed;

        if (sleep_time < 0)
            sleep_time = 0;

        poll(fds, 1, sleep_time);

        if (fds[0].revents & POLLIN) {
            while (ping_recv_msg() == 0)
                ;

            if (total_msgs_to_send > 0 && stats.recv_packets + stats.expired_packets == total_msgs_to_send)
                exit_ping = 1;
        }
    }

    sigaction(SIGINT, &old, NULL);
}

int main(int argc, char **argv)
{
    enum arg_index ret;
    struct sockaddr_in *in;
    struct timeval start, end;
    const char *dest = NULL;
    int sent_percent = 0;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_count:
            total_msgs_to_send = atoi(argarg);
            break;

        case ARG_interval:
            interval_ms = atoi(argarg);
            break;

        case ARG_flood:
            flood_display = 1;
            break;

        case ARG_EXTRA:
            if (!dest) {
                dest = argarg;
            } else {
                printf("%s: Unreconized arg \"%s\"\n", argv[0], argarg);
                return 1;
            }
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    msg_id = getpid();

    ping_fd = socket(AF_INET, SOCK_RAW | SOCK_NONBLOCK, IPPROTO_ICMP);
    if (ping_fd == -1) {
        perror("socket");
        return 1;
    }

    in = (struct sockaddr_in *)&dest_addr;
    in->sin_family = AF_INET;
    in->sin_addr.s_addr = inet_addr(dest);

    printf("Ping: %s\n", inet_ntoa(in->sin_addr));

    gettimeofday(&start, NULL);
    ping_loop();
    gettimeofday(&end, NULL);

    if (stats.sent_packets)
        sent_percent = ((stats.sent_packets - stats.recv_packets) * 100) / stats.sent_packets;

    printf("--- %s ping statistics ---\n", dest);
    printf("%d packets sent, %d received, %d expired, %d%% packet loss, total time %dms\n",
            stats.sent_packets,
            stats.recv_packets,
            stats.expired_packets,
            sent_percent,
            timeval_elapsed_ms(start, end));

    close(ping_fd);

    return 0;
}

