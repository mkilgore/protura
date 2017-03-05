
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <errno.h>
#include <protura/syscall.h>
#include <protura/net/sockaddr.h>
#include <protura/net/af/ipv4.h>

#define QQ(x) #x
#define Q(x) QQ(x)

#define htonl __khtonl
#define ntohl __kntohl
#define htons __khtons
#define ntohs __kntohs

static inline int syscall6(int sys, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
    int out;
    asm volatile(
                "push %%ebp\n"
                "mov %7, %%ebp\n"
                "int $" Q(INT_SYSCALL) "\n"
                "pop %%ebp\n"
                 : "=a" (out)
                 : "0" (sys), "b" (arg1), "c" (arg2), "d" (arg3), "S" (arg4), "D" (arg5), "m" (arg6)
                 : "memory");

    return out;
}

static inline int syscall5(int sys, int arg1, int arg2, int arg3, int arg4, int arg5)
{
    int out;
    asm volatile("int $" Q(INT_SYSCALL) "\n"
                 : "=a" (out)
                 : "0" (sys), "b" (arg1), "c" (arg2), "d" (arg3), "S" (arg4), "D" (arg5)
                 : "memory");

    return out;
}

static inline int syscall4(int sys, int arg1, int arg2, int arg3, int arg4)
{
    int out;
    asm volatile("int $" Q(INT_SYSCALL) "\n"
                 : "=a" (out)
                 : "0" (sys), "b" (arg1), "c" (arg2), "d" (arg3), "S" (arg4)
                 : "memory");

    return out;
}

static inline int syscall3(int sys, int arg1, int arg2, int arg3)
{
    int out;
    asm volatile("int $" Q(INT_SYSCALL) "\n"
                 : "=a" (out)
                 : "0" (sys), "b" (arg1), "c" (arg2), "d" (arg3)
                 : "memory");

    return out;
}

static inline int syscall2(int sys, int arg1, int arg2)
{
    int out;
    asm volatile("int $" Q(INT_SYSCALL) "\n"
                 : "=a" (out)
                 : "0" (sys), "b" (arg1), "c" (arg2)
                 : "memory");

    return out;
}

static inline int syscall1(int sys, int arg1)
{
    int out;
    asm volatile("int $" Q(INT_SYSCALL) "\n"
                 : "=a" (out)
                 : "0" (sys), "b" (arg1)
                 : "memory");

    return out;
}

static inline int syscall0(int sys)
{
    int out;
    asm volatile("int $" Q(INT_SYSCALL) "\n"
                 : "=a" (out)
                 : "0" (sys)
                 : "memory");

    return out;
}

in_addr_t inet_addr(const char *ip)
{
    int vals[4];
    int i;

    for (i = 0; i < 4; i++) {
        int new_val = 0;
        while (*ip >= '0' && *ip <= '9') {
            new_val = new_val * 10 + (*ip - '0');
            ip++;
        }

        vals[i] = new_val;
        ip++;
    }

    return (vals[3] << 24) + (vals[2] << 16) + (vals[1] << 8) + vals[0];
}

int socket(int af, int type, int proto)
{
    return syscall3(SYSCALL_SOCKET, af, type, proto);
}

int sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
    return syscall6(SYSCALL_SENDTO, sockfd, (int)buf, (int)len, flags, (int)dest_addr, (int)addrlen);
}

int recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
    return syscall6(SYSCALL_RECVFROM, sockfd, (int)buf, (int)len, flags, (int)src_addr, (int)addrlen);
}

int bind(int sockfd, const struct sockaddr *bind_addr, socklen_t addrlen)
{
    return syscall3(SYSCALL_BIND, sockfd, (int)bind_addr, (int)addrlen);
}

int old_main(int argc, char **argv)
{
    int sockfd;
    struct sockaddr_in in;
    const char msg[] = "Hello, UDP!";
    const char dns[] = {
                    0x24, 0x1a, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x77,
        0x77, 0x77, 0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c,
        0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01,
        0x00, 0x01
    };
    int ret;

    sockfd = socket(AF_INET, 0, 0);
    printf("Socket: %d\n", sockfd);

    memset(&in, 0, sizeof(in));
    in.sin_family = AF_INET;
    in.sin_port = htons(5000);
    in.sin_addr.s_addr = htonl(0xC0A80101);

    printf("Offsetof: %zd\n", offsetof(struct sockaddr_in, sin_addr.s_addr));

    ret = sendto(sockfd, msg, sizeof(msg), 0, (struct sockaddr *)&in, sizeof(in));
    printf("sendto: %d\n", ret);

    memset(&in, 0, sizeof(in));
    in.sin_family = AF_INET;
    in.sin_port = htons(53);
    in.sin_addr.s_addr = htonl(0x08080808);

    ret = sendto(sockfd, dns, sizeof(dns), 0, (struct sockaddr *)&in, sizeof(in));
    printf("sendto: %d\n", ret);

    return 0;
}

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*a))

int main(int argc, char **argv)
{
    int s;
    short inport;
    short outport;
    struct sockaddr_in addr;
    struct sockaddr_in inaddr;
    struct pollfd pollfd[2];

    memset(&addr, 0, sizeof(addr));

    if (argc < 4) {
        printf("Usage: %s [ip_address] [outport] [inport]\n", argv[0]);
        return 0;
    }

    inport = atoi(argv[2]);
    outport = atoi(argv[3]);

    printf("Connecting to %s:%d from %d\n", argv[1], outport, inport);

    s = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(outport);
    addr.sin_addr.s_addr = inet_addr(argv[1]);

    inaddr.sin_family = AF_INET;
    inaddr.sin_port = htons(inport);
    inaddr.sin_addr.s_addr = INADDR_ANY;

    bind(s, (struct sockaddr *)&inaddr, sizeof(inaddr));

    printf("Now Chating!\n");

    pollfd[0].fd = STDIN_FILENO;
    pollfd[0].events = POLLIN;

    pollfd[1].fd = s;
    pollfd[1].events = POLLIN;

    for (;;) {
        int ret;
        char buf[256];

        poll(pollfd, ARRAY_SIZE(pollfd), -1);

        if (pollfd[0].revents & POLLIN) {
            fgets(buf, sizeof(buf), stdin);

            sendto(s, buf, strlen(buf) - 1, 0, (struct sockaddr *)&addr, sizeof(addr));
        }

        if (pollfd[1].revents & POLLIN) {
            ret = recvfrom(s, buf, sizeof(buf), 0, NULL, NULL);
            if (ret > 0) {
                printf("%.*s\n", ret, buf);
            } else {
                printf("Err: %d\n", ret);
            }
        }
    }

    return 0;
}

