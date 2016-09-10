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
    int ret, i;
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

    for (i = 1; i <= NSIG; i++)
        signal(i, SIG_DFL);

    sigemptyset(&blocked);
    sigprocmask(SIG_SETMASK, &blocked, NULL);

    if ((ret = execvp(prog->file, prog->argv)) == -1) {
        perror(prog->file);
        exit(1);
    }

    printf("Uhhh, execvp returned: %d...\n", ret);
    exit(1);
}

int prog_start(const struct prog_desc *prog, pid_t *child_pid)
{
    sigset_t original_set, blocked;

    sigfillset(&blocked);
    sigprocmask(SIG_SETMASK, &blocked, &original_set);

    pid_t pid = fork_pgrp(prog->pgid);

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

    sigprocmask(SIG_SETMASK, &original_set, NULL);

    return 0;
}

