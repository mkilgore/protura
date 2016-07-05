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
/*
#include <readline/readline.h>
#include <readline/history.h>
*/

#include "shell.h"

#define INPUT_MAX 100

static void input_new_line(const char *prompt, char *buf, size_t len)
{
    size_t loc = 0;
    memset(buf, 0, len);

    printf("%s", prompt);

    while ((buf[loc] = getchar()) != '\n') {
        putchar(buf[loc]);
        if (loc < len - 1)
            loc++;
    }

    putchar('\n');

    buf[loc + 1] = '\0';
}

void input_loop(void)
{
    const char *prompt = "$ ";
    int exit_loop = 0;
    char line[INPUT_MAX + 1];

    do {
        putchar('\n');
        input_new_line(prompt, line, sizeof(line));
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
    } while (!exit_loop);

    /*
    clear_history();
    */

    return ;
}
