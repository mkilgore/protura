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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include "builtin.h"

struct builtin_cmd {
    const char *id;
    int (*cmd) (struct prog_desc *);
};

static int cd(struct prog_desc *prog)
{
    int ret;

    if (prog->argc <= 1)
        return 1;

    printf("cd: %s\n", prog->argv[1]);

    ret = chdir(prog->argv[1]);

    if (ret == -1)
        printf("Error changing directory: %s\n", strerror(errno));

    return 0;
}

static struct builtin_cmd cmds[] = {
    { .id = "cd", .cmd = cd },
    { .id = NULL, .cmd = NULL }
};

int builtin_exec(struct prog_desc *prog, int *ret)
{
    struct builtin_cmd *cmd = cmds;

    if (!prog->file)
        return 1;

    for (; cmd->id; cmd++) {
        if (strcmp(prog->file, cmd->id) == 0) {
            int k;

            k = (cmd->cmd) (prog);

            if (ret)
                *ret = k;

            return 0;
        }
    }

    return 1;
}

