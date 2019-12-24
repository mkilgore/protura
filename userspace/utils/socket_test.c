
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

int main(int argc, char **argv)
{
    int s;
    short inport;
    struct sockaddr_in inaddr;
    struct pollfd pollfd[2];
    int is_connected = 0;

    if (argc != 2) {
        printf("Usage: %s [inport]\n", argv[0]);
        return 0;
    }

    inport = atoi(argv[1]);

    printf("%s: Listening on port: %d\n", argv[1], inport);

    s = socket(AF_INET, SOCK_DGRAM, 0);

    inaddr.sin_family = AF_INET;
    inaddr.sin_port = htons(inport);
    inaddr.sin_addr.s_addr = INADDR_ANY;

    bind(s, (struct sockaddr *)&inaddr, sizeof(inaddr));

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

            if (is_connected)
                send(s, buf, strlen(buf), 0);
            else
                printf("Can't send, not connected!\n");
        }

        if (pollfd[1].revents & POLLIN) {
            struct sockaddr_in addr;
            socklen_t addr_len = sizeof(addr);
            memset(&addr, 0, sizeof(addr));

            ret = recvfrom(s, buf, sizeof(buf), 1, (struct sockaddr *)&addr, &addr_len);

            if (ret > 0) {
                if (!is_connected) {
                    ret = connect(s, (struct sockaddr *)&addr, addr_len);
                    if (ret)
                        printf("connect err: %d, %s\n", ret, strerror(errno));
                    else {
                        printf("Connected to: %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
                        printf("Chatting...\n");
                        is_connected = 1;
                    }
                }

                printf("%.*s", ret, buf);
            } else {
                printf("recvfrom err: %d\n", ret);
            }
        }
    }

    return 0;
}

