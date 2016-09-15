
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <poll.h>

#define PREAD 0
#define PWRITE 1

#define PIPE_COUNT 10

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*(arr)))

int pipe_fd[PIPE_COUNT][2];

void child(void)
{
    int i;
    int k = 2;

    for (i = 0; i < PIPE_COUNT; i++) {
        printf("Child sleeping...\n");
        sleep(1);

        printf("Child Sending signal: %d!\n", i);
        write(pipe_fd[i][PWRITE], &k, sizeof(k));

        close(pipe_fd[i][PWRITE]);
    }
}

void wait_for_signal(void)
{
    int i, k;
    int ret;
    struct pollfd fds[1 + PIPE_COUNT];

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    for (i = 0; i < PIPE_COUNT; i++) {
        fds[i + 1].fd = pipe_fd[i][PREAD];
        fds[i + 1].events = POLLIN;
        fds[i + 1].revents = 0;
    }

    for (i = 0; i < PIPE_COUNT; i++) {
        printf("Polling...\n");

        ret = poll(fds, ARRAY_SIZE(fds), -1);

        printf("Poll returned: %d!!!\n", ret);

        for (k = 0; k < ARRAY_SIZE(fds); k++) {
            printf("revents %d: %d\n", k, fds[k].revents);
            if (fds[k].revents)
                fds[k].fd = -1;
        }
    }

    return ;
}

void wait_for_keyboard(void)
{
    int ret;

    printf("Sleeping for 2 seconds using poll...\n");

    ret = poll(NULL, 0, 2000);

    printf("Ret: %d\n", ret);
}

void fork_child(void)
{
    switch (fork()) {
    case 0:
        child();
        exit(0);

    default:
        break;
    }
}

int main(int argc, char **argv, char **envp)
{
    int i;
    int ret;

    for (i = 0; i < PIPE_COUNT; i++) {
        ret = pipe(pipe_fd[i]);
        if (ret == -1) {
            perror("pipe");
            return 1;
        }
        fcntl(pipe_fd[i][PREAD], F_SETFD, fcntl(pipe_fd[i][PREAD], F_GETFD) | FD_CLOEXEC);
    }

    printf("Forking cihld...\n");

    fork_child();

    for (i = 0; i < PIPE_COUNT; i++)
        close(pipe_fd[i][PWRITE]);

    wait_for_signal();
    wait_for_keyboard();

    wait(NULL);

    return 0;
}

