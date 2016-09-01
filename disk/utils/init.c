
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <protura/syscall.h>
#include <sys/mount.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*(arr)))

pid_t shell[2];

/* Reap children */
static void handle_children(int sig)
{
    pid_t child;
    printf("Waiting on children!\n");

    while ((child = waitpid(-1, NULL, WNOHANG)) > 0) {
        printf("Reaped %d\n", child);

        int i;
        for (i = 0; i < ARRAY_SIZE(shell); i++) {
            if (shell[i] == child) {
                printf("Syncing...\n");
                sync();
            }
        }
    }
}

static pid_t start_prog(const char *prog, char *const argv[], char *const envp[])
{
    pid_t child_pid;

    switch ((child_pid = fork_pgrp(0))) {
    case -1:
        /* Fork error */
        return -1;

    case 0:
        /* In child */
        execve(prog, argv, envp);
        exit(0);

    default:
        /* In parent */
        return child_pid;
    }
}

int main(int argc, char **argv)
{
    int consolefd, keyboardfd, stderrfd;
    int ret;
    struct sigaction action;

    memset(&action, 0, sizeof(action));


    action.sa_handler = handle_children;
    sigaction(SIGCHLD, &action, NULL);

    keyboardfd = open("/dev/console", O_RDONLY);
    consolefd = open("/dev/console", O_WRONLY);
    stderrfd = open("/dev/console", O_WRONLY);

    /* Mount proc if we can */
    ret = mount(NULL, "/proc", "proc", 0, NULL);
    if (ret)
        perror("mount proc");

    setpgid(0, 0);

    shell[0] = start_prog("/bin/sh", NULL, (char *const[]) { "PATH=/bin", NULL });

    close(keyboardfd);
    close(consolefd);
    close(stderrfd);

    keyboardfd = open("/dev/com2", O_RDONLY);
    consolefd = open("/dev/com2", O_WRONLY);
    stderrfd = open("/dev/com2", O_WRONLY);

    shell[1] = start_prog("/bin/sh", NULL, (char *const[]) { "PATH=/bin", NULL });

    /* Sleep forever */
    while (1)
        pause();

    return 0;
}

