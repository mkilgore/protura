
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define QQ(x) #x
#define Q(x) QQ(x)

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*a))

enum {
    STDIN_FD,
    SOCKET_FD,
};

static char buf[4096 * 2];

int main(int argc, char **argv)
{
    int s;
    struct sockaddr_in inaddr;

    if (argc != 3) {
        printf("Usage: %s [ip] [inport]\n", argv[0]);
        return 0;
    }

    inaddr.sin_family = AF_INET;
    inaddr.sin_addr.s_addr = inet_addr(argv[1]);
    inaddr.sin_port= htons(atoi(argv[2]));

    s = socket(AF_INET, SOCK_STREAM, 0);

    int ret = connect(s, (struct sockaddr *)&inaddr, sizeof(inaddr));
    printf("Connect ret: %d\n", ret);
    if (ret == -1) {
        printf("  Error: %s\n", strerror(errno));
        return 1;
    }

    struct pollfd pollfds[] = {
        [STDIN_FD]  = { .fd = STDIN_FILENO, .events = POLLIN, .revents = 0 },
        [SOCKET_FD] = { .fd = s,            .events = POLLIN, .revents = 0 },
    };

    while (1) {
        int i;
        for (i = 0; i < ARRAY_SIZE(pollfds); i++)
            pollfds[i].revents = 0;

        ret = poll(pollfds, ARRAY_SIZE(pollfds), -1);

        if (pollfds[SOCKET_FD].revents & POLLIN) {
            /* make sure buf is NUL terminated */
            memset(buf, 0, sizeof(buf));
            ssize_t len = read(s, buf, sizeof(buf) - 1);

            if (len == -1) {
                printf("\npoll error: %s\n", strerror(errno));
                break;
            } else if (len) {
                printf("%s", buf);
            } else {
                printf("\nSOCKET EOF!\n");
                return 0;
            }
        }

        if (pollfds[STDIN_FD].revents & POLLIN) {
            /* make sure buf is NUL terminated */
            memset(buf, 0, sizeof(buf));
            ssize_t len = read(STDIN_FILENO, buf, sizeof(buf) - 1);

            if (len) {
                ssize_t write_ret = write(s, buf, len);
                if (write_ret == -1)
                    printf("\nwrite error: %s\n", strerror(errno));
            } else {
                /* Stop polling stdin when we get an EOF */
                pollfds[STDIN_FD].fd = -1;
            }
        }
    }

    return 0;
}
