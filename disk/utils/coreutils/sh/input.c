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

#include "shell.h"

#define INPUT_MAX 100

static int interactive = 0;
static FILE *inp_file;

struct job *current_job;

static ssize_t get_input(char **line, size_t *buf_len)
{
    ssize_t len;

    if (interactive)
        printf("%s $ ", cwd);

    len = getline(line, buf_len, inp_file);

    (*line)[len - 1] = '\0';
    len--;

    return len;
}

static void input_loop(void)
{
    char *line = NULL;
    size_t buf_len = 0;
    struct job *job;

    while (!feof(inp_file)) {
        job_update_background();

        if (current_job) {
            job_make_forground(current_job);
            current_job = NULL;
            continue;
        }

        get_input(&line, &buf_len);

        if (strcmp(line, "exit") == 0)
            break;

        job = shell_parse_job(line);

        if (!job)
            continue;

        if (job_is_simple_builtin(job)) {
            job_builtin_run(job);
            job_clear(job);
            free(job);
            continue;
        }

        job_add(job);
        job_first_start(job);
        current_job = job;
    }

    free(line);

    return ;
}

void keyboard_input_loop(void)
{
    interactive = 1;
    inp_file = stdin;
    input_loop();
}

void script_input_loop(int scriptfile)
{
    inp_file = fdopen(scriptfile, "r");
    input_loop();
}

