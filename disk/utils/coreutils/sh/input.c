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
/*
#include <readline/readline.h>
#include <readline/history.h>
*/

#include "shell.h"

#define INPUT_MAX 100

static void input_new_line(char *buf, size_t len)
{
    size_t loc = 0;
    memset(buf, 0, len);

    printf("%s $ ", cwd);
    fgets(buf, len, stdin);
}

void input_loop(void)
{
    int exit_loop = 0;
    char line[INPUT_MAX + 1];

    do {
        input_new_line(line, sizeof(line));
        /*
        line = readline(prompt);
        if (line && *line)
            add_history(line);
        else
            continue;
            */

        if (strcmp(line, "exit") == 0)
            exit_loop = 1;
        else
            shell_run_line(line);
    } while (!exit_loop && !feof(stdin));

    /*
    clear_history();
    */

    return ;
}

void input_script_loop(int fd)
{
    char *line = NULL;
    size_t buf_len = 0;
    int len;
    FILE *fin = fdopen(fd, "r");

    while ((len = getline(&line, &buf_len, fin)) != -1) {
        if (strcmp(line, "exit") == 0)
            break;
        else
            shell_run_line(line);
    }

    free(line);

    return ;
}

