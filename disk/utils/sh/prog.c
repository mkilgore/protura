/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "prog.h"

static void start_child(const struct prog_desc *prog)
{
    if (prog->stdin_fd != STDIN_FILENO)
        dup2(prog->stdin_fd, STDIN_FILENO);

    if (prog->stdout_fd != STDOUT_FILENO)
        dup2(prog->stdout_fd, STDOUT_FILENO);

    if (prog->stderr_fd != STDERR_FILENO)
        dup2(prog->stderr_fd, STDERR_FILENO);

    int i;
    for (i = 3; i < 20; i++)
        close(i);

    if (execvp(prog->file, prog->argv) == -1) {
        printf("Error execing program: %s\n", prog->file);
        printf("Error: %s\n", strerror(errno));
        exit(0);
    }

    printf("Uhhh, execvp returned...\n");
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
    return 0;
}

