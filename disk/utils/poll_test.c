
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/select.h>
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
    int order[PIPE_COUNT];

    for (i = 0; i < PIPE_COUNT; i++)
        order[i] = i;

    for (i = 0; i < 100; i++) {
        int o1 = rand() % PIPE_COUNT, o2 = rand() % PIPE_COUNT;
        int tmp;
        tmp = order[o1];
        order[o1] = order[o2];
        order[o2] = tmp;
    }

    for (i = 0; i < PIPE_COUNT; i++) {
        printf("Child sleeping...\n");
        sleep(1);

        printf("Child Sending signal: %d!\n", order[i]);
        write(pipe_fd[order[i]][PWRITE], &k, sizeof(k));

        //close(pipe_fd[order[i]][PWRITE]);
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
            if (fds[k].revents) {
                int res;
                printf("Reading once\n");

                ret = read(fds[k].fd, &res, sizeof(res));
                printf("res: %d\n", res);
                printf("ret: %d\n", ret);
                printf("Reading again...\n");

                ret = read(fds[k].fd, &res, sizeof(res));
                printf("res: %d\n", res);
                printf("ret: %d - err: %d(%s)\n", ret, errno, strerror(errno));

                fds[k].fd = -1;
                fds[k].revents = 0;
            }
        }
    }

    return ;
}

void select_test(void)
{
    int ret;
    fd_set read;
    struct timeval tim;

    FD_ZERO(&read);
    FD_SET(STDIN_FILENO, &read);

    tim.tv_sec = 5;
    tim.tv_usec = 0;

    printf("Select...\n");
    ret = select(STDIN_FILENO + 1, &read, NULL, NULL, &tim);

    printf("ret: %d\n", ret);
    printf("is_set: %d\n", FD_ISSET(STDIN_FILENO, &read));
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

    srand(time(NULL));

    for (i = 0; i < PIPE_COUNT; i++) {
        ret = pipe(pipe_fd[i]);
        if (ret == -1) {
            perror("pipe");
            return 1;
        }
        fcntl(pipe_fd[i][PREAD], F_SETFD, fcntl(pipe_fd[i][PREAD], F_GETFD) | FD_CLOEXEC);

        fcntl(pipe_fd[i][PREAD], F_SETFL, fcntl(pipe_fd[i][PREAD], F_GETFL) | O_NONBLOCK);
        fcntl(pipe_fd[i][PWRITE], F_SETFL, fcntl(pipe_fd[i][PWRITE], F_GETFL) | O_NONBLOCK);
    }

    printf("Forking child...\n");

    fork_child();

    for (i = 0; i < PIPE_COUNT; i++)
        close(pipe_fd[i][PWRITE]);

    wait_for_signal();
    wait_for_keyboard();
    select_test();

    wait(NULL);

    return 0;
}

