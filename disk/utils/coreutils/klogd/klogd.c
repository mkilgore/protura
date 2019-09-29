
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/poll.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*(arr)))

#define KLOGFILE "/proc/klog"
#define KLOG_OUTPUT "/var/log/klog"

const char *prog_name;

static int make_daemon(void)
{
    switch (fork()) {
    case 0:
        break;

    case -1:
        perror("fork()");
        return 1;

    default:
        exit(0);
    }

    /* get a new session id to disconnect us from the current tty */
    setsid();

    /* Change our current directory */
    chdir("/");

    /* Fork again so we're no longer a session leader */
    switch (fork()) {
    case 0:
        break;

    case -1:
        perror("fork()");
        return 1;

    default:
        exit(0);
    }

    int devnull = open("/dev/null", O_RDWR);

    dup2(devnull, STDIN_FILENO);
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);

    /* if the devnull fd wasn't one of stdin/stdout/stderr, close it */
    if (devnull > 2)
        close(devnull);

    return 0;
}

static char copy_buffer[4096 * 8];

static void poll_klog(int klog_fd, int output_fd)
{
    struct pollfd pollfd = { .fd = klog_fd };

    while (1) {
        pollfd.events = POLLIN;
        pollfd.revents = 0;

        poll(&pollfd, 1, -1);

        ssize_t ret;
        while ((ret = read(klog_fd, copy_buffer, sizeof(copy_buffer)))) {
            if (ret == -1)
                break;

            write(output_fd, copy_buffer, ret);
        }

        sync();
    }
}

int main(int argc, char **argv)
{
    prog_name = argv[0];

    int ret = make_daemon();
    if (ret)
        return 1;

    int klog_fd = open(KLOGFILE, O_RDONLY | O_NONBLOCK);
    int output_fd = open(KLOG_OUTPUT, O_RDWR | O_CREAT | O_APPEND, 0777);

    poll_klog(klog_fd, output_fd);

    return 0;
}

