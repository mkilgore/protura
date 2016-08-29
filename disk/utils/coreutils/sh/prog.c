/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

// sh - shell, command line interpreter
#define UTILITY_NAME "sh"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "prog.h"

static void start_child(const struct prog_desc *prog)
{
    int ret;
    sigset_t blocked;

    if (prog->stdin_fd != STDIN_FILENO) {
        dup2(prog->stdin_fd, STDIN_FILENO);
        close(prog->stdin_fd);
    }

    if (prog->stdout_fd != STDOUT_FILENO) {
        dup2(prog->stdout_fd, STDOUT_FILENO);
        close(prog->stdout_fd);
    }

    if (prog->stderr_fd != STDERR_FILENO) {
        dup2(prog->stderr_fd, STDERR_FILENO);
        close(prog->stderr_fd);
    }

    sigemptyset(&blocked);
    sigprocmask(SIG_SETMASK, &blocked, NULL);

    if ((ret = execvp(prog->file, prog->argv)) == -1) {
        perror(prog->file);
        exit(0);
    }

    printf("Uhhh, execvp returned: %d...\n", ret);
    exit(0);
}

int prog_start(const struct prog_desc *prog, pid_t *child_pid)
{
    pid_t pid = fork();

    if (pid == -1)
        return 1; /* fork() returned an error - abort */

    if (pid == 0) /* 0 is returned to the child process */
        start_child(prog);

    *child_pid = pid;

    /* Now that prog is started, we close the original inputs */
    if (prog->stdin_fd != STDIN_FILENO)
        close(prog->stdin_fd);

    if (prog->stdout_fd != STDOUT_FILENO)
        close(prog->stdout_fd);

    if (prog->stderr_fd != STDERR_FILENO)
        close(prog->stderr_fd);

    return 0;
}

