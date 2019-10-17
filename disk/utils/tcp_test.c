
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

    char buf[11];

    while (1) {
        struct pollfd pollfd = { .fd = s, .events = POLLIN, .revents = 0 };

        ret = poll(&pollfd, 1, -1);

        /* make sure buf is NUL terminated */
        memset(buf, 0, sizeof(buf));
        ssize_t len = read(s, buf, sizeof(buf) - 1);

        if (len == -1) {
            printf("\npoll error: %s\n", strerror(errno));
            break;
        } else {
            printf("%s", buf);
        }
    }

    return 0;
}
